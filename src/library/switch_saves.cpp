#include "switch_saves.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstdint>
#include <format>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <system_error>

#include <nlohmann/json.hpp>

#include "lucent/log.h"

namespace opensu::library::saves {
namespace {

namespace fs = std::filesystem;

/// yuzu's user saves sit under save id 0.
constexpr std::string_view yuzuUserSaves = "user/save/0000000000000000";
/// yuzu files device saves under the all-zero user.
constexpr std::string_view yuzuDeviceUser = "00000000000000000000000000000000";
constexpr std::string_view yuzuProfiles = "system/save/8000000000000010/su/avators/profiles.dat";
/// profiles.dat: 0x10 bytes of padding, then eight 0xC8-byte users, each starting with its UUID.
constexpr std::size_t profilesHeader = 0x10;
constexpr std::size_t profileSize = 0xC8;
constexpr std::size_t profileCount = 8;
constexpr std::size_t uuidSize = 16;
constexpr std::string_view yuzuCurrentUser = "current_user=";

constexpr std::string_view ryujinxUserSaves = "user/save";
constexpr std::string_view ryujinxProfiles = "system/Profiles.json";
/// LibHac's SaveDataExtraData: the SaveDataAttribute first, 0x200 bytes in all.
constexpr std::string_view extraData = "ExtraData0";
constexpr std::uintmax_t extraDataSize = 0x200;
constexpr std::size_t attributeProgramId = 0x00;
constexpr std::size_t attributeUserHigh = 0x08;
constexpr std::size_t attributeUserLow = 0x10;
constexpr std::size_t attributeType = 0x20;
constexpr std::size_t attributeRank = 0x21;
constexpr std::size_t attributeIndex = 0x22;
/// SaveDataType: Account 1, Device 3.
constexpr std::uint8_t saveTypeAccount = 1;
constexpr std::uint8_t saveTypeDevice = 3;
/// DirectorySaveDataFileSystem's committed and working folders.
constexpr std::string_view committed = "0";
constexpr std::string_view working = "1";

std::string readBytes(const fs::path& file) {
    std::ifstream in{file, std::ios::binary};
    return {std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}};
}

std::uint64_t littleEndian(std::string_view bytes, std::size_t at, std::size_t size) {
    std::uint64_t value = 0;
    for (std::size_t i = size; i > 0; --i) {
        value = (value << 8U) | static_cast<unsigned char>(bytes[at + i - 1]);
    }
    return value;
}

std::string hex16(std::uint64_t value) {
    return std::format("{:016X}", value);
}

std::optional<std::uint64_t> titleNumber(std::string_view titleId) {
    std::uint64_t value = 0;
    const auto [end, error] =
        std::from_chars(titleId.data(), titleId.data() + titleId.size(), value, 16);
    if (error != std::errc{} || end != titleId.data() + titleId.size()) {
        return std::nullopt;
    }
    return value;
}

std::vector<fs::path> folders(const fs::path& parent) {
    std::vector<fs::path> found;
    std::error_code ec;
    for (const fs::directory_entry& entry : fs::directory_iterator{parent, ec}) {
        if (entry.is_directory(ec) && !entry.is_symlink(ec)) {
            found.push_back(entry.path());
        }
    }
    std::ranges::sort(found);
    return found;
}

/// The user folders of profiles.dat in its order, as yuzu names them: the UUID's second 64 bits
/// then its first, each upper-case hex.
std::vector<std::string> yuzuUsers(const fs::path& nand) {
    const std::string bytes = readBytes(nand / yuzuProfiles);
    std::vector<std::string> users;
    for (std::size_t i = 0; i < profileCount; ++i) {
        const std::size_t at = profilesHeader + (i * profileSize);
        if (bytes.size() < at + uuidSize) {
            break;
        }
        const std::uint64_t first = littleEndian(bytes, at, sizeof(std::uint64_t));
        const std::uint64_t second =
            littleEndian(bytes, at + sizeof(std::uint64_t), sizeof(std::uint64_t));
        if (first != 0 || second != 0) {
            users.push_back(hex16(second) + hex16(first));
        }
    }
    return users;
}

/// qt-config.ini's `current_user`, an index into the profiles; 0 when it names none.
std::size_t yuzuCurrentIndex(const fs::path& config) {
    std::ifstream in{config / "qt-config.ini"};
    std::string line;
    while (std::getline(in, line)) {
        if (!line.starts_with(yuzuCurrentUser)) {
            continue;
        }
        std::size_t index = 0;
        const std::string_view value = std::string_view{line}.substr(yuzuCurrentUser.size());
        const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), index);
        return error == std::errc{} ? index : 0;
    }
    return 0;
}

/// The open profile's user id in Profiles.json, upper-case; empty when none is open.
std::string ryujinxCurrentUser(const fs::path& config) {
    const nlohmann::json root =
        nlohmann::json::parse(readBytes(config / ryujinxProfiles), nullptr, false);
    if (root.is_discarded() || !root.is_object() || !root.contains("profiles") ||
        !root["profiles"].is_array()) {
        return {};
    }
    for (const nlohmann::json& profile : root["profiles"]) {
        if (profile.is_object() && profile.value("account_state", "") == "Open" &&
            profile.contains("user_id") && profile["user_id"].is_string()) {
            std::string user = profile["user_id"].get<std::string>();
            std::ranges::transform(user, user.begin(), [](unsigned char c) {
                return static_cast<char>(std::toupper(c));
            });
            return user;
        }
    }
    return {};
}

/// The slot `folder`'s ExtraData0 describes, when it is `title`'s primary account or device save.
std::optional<SaveSlot> ryujinxSlot(const fs::path& folder, std::uint64_t title) {
    std::error_code ec;
    const fs::path extra = folder / extraData;
    if (fs::file_size(extra, ec) != extraDataSize || ec) {
        return std::nullopt;
    }
    const std::string bytes = readBytes(extra);
    if (bytes.size() != extraDataSize ||
        littleEndian(bytes, attributeProgramId, sizeof(std::uint64_t)) != title ||
        littleEndian(bytes, attributeRank, 1) != 0 || littleEndian(bytes, attributeIndex, 2) != 0) {
        return std::nullopt;
    }
    const auto type = static_cast<std::uint8_t>(littleEndian(bytes, attributeType, 1));
    if (type != saveTypeAccount && type != saveTypeDevice) {
        return std::nullopt;
    }
    SaveSlot slot;
    slot.kind = type == saveTypeAccount ? SlotKind::Account : SlotKind::Device;
    if (type == saveTypeAccount) {
        slot.user = hex16(littleEndian(bytes, attributeUserHigh, sizeof(std::uint64_t))) +
                    hex16(littleEndian(bytes, attributeUserLow, sizeof(std::uint64_t)));
    }
    slot.root = folder;
    // The committed folder is what the emulator mounts; the working one is synced from it.
    const bool committedThere = fs::is_directory(folder / committed, ec);
    slot.content = committedThere || !fs::is_directory(folder / working, ec)
                       ? std::vector<fs::path>{folder / committed, folder / working}
                       : std::vector<fs::path>{folder / working, folder / committed};
    return slot;
}

} // namespace

std::vector<SaveSlot> yuzuSlots(const fs::path& nand, const fs::path& config,
                                std::string_view titleId) {
    const std::vector<std::string> users = yuzuUsers(nand);
    const std::size_t index = yuzuCurrentIndex(config);
    const std::string current = index < users.size() ? users[index] : std::string{};
    std::vector<SaveSlot> slots;
    for (const fs::path& user : folders(nand / yuzuUserSaves)) {
        const fs::path save = user / std::string{titleId};
        std::error_code ec;
        if (!fs::is_directory(save, ec)) {
            continue;
        }
        const std::string name = user.filename().string();
        SaveSlot slot;
        slot.kind = name == yuzuDeviceUser ? SlotKind::Device : SlotKind::Account;
        if (slot.kind == SlotKind::Account) {
            slot.user = name;
            slot.current = name == current;
        }
        slot.root = save;
        slot.content = {save};
        slots.push_back(std::move(slot));
    }
    return slots;
}

std::vector<SaveSlot> ryujinxSlots(const fs::path& bis, const fs::path& config,
                                   std::string_view titleId) {
    const std::optional<std::uint64_t> title = titleNumber(titleId);
    if (!title) {
        lucent::warn("saves", "{} is not a Switch title id", titleId);
        return {};
    }
    const std::string current = ryujinxCurrentUser(config);
    std::vector<SaveSlot> slots;
    const auto add = [&](const fs::path& folder) {
        if (std::optional<SaveSlot> slot = ryujinxSlot(folder, *title)) {
            slot->current = slot->kind == SlotKind::Account && slot->user == current;
            slots.push_back(std::move(*slot));
        }
    };
    std::error_code ec;
    for (const fs::path& outer : folders(bis / ryujinxUserSaves)) {
        // Ryubing names a save folder by its save id; a build that files saves by game puts them
        // one level down, under the title id.
        if (fs::exists(outer / extraData, ec)) {
            add(outer);
            continue;
        }
        for (const fs::path& inner : folders(outer)) {
            add(inner);
        }
    }
    return slots;
}

} // namespace opensu::library::saves
