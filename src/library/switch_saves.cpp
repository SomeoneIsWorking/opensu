#include "switch_saves.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstdint>
#include <format>
#include <fstream>
#include <iterator>
#include <map>
#include <optional>
#include <random>
#include <string>
#include <system_error>
#include <tuple>
#include <utility>

#include <nlohmann/json.hpp>

#include "fileio/atomic_write.hpp"
#include "fileio/little_endian.hpp"
#include "ini_file.hpp"
#include "lucent/log.h"
#include "yuzu_config.hpp"

namespace opensu::library::saves {
namespace {

namespace fs = std::filesystem;

/// yuzu's user saves sit under save id 0.
constexpr std::string_view yuzuUserSaves = "user/save/0000000000000000";
/// yuzu files device saves under the all-zero user.
constexpr std::string_view yuzuDeviceUser = "00000000000000000000000000000000";
/// Eden's newer layout, used for a save where it is there: `account/<UUID bytes>/<title id>/0`
/// and `device/<title id>/0` under `user/save`.
constexpr std::string_view edenAccountSaves = "user/save/account";
constexpr std::string_view edenDeviceSaves = "user/save/device";
constexpr std::string_view edenSaveFolder = "0";
constexpr std::string_view yuzuProfiles = "system/save/8000000000000010/su/avators/profiles.dat";
/// profiles.dat: 0x10 bytes of padding, then eight 0xC8-byte users, each starting with its UUID.
constexpr std::size_t profilesHeader = 0x10;
constexpr std::size_t profileSize = 0xC8;
constexpr std::size_t profileCount = 8;
constexpr std::size_t uuidSize = 16;
constexpr std::string_view yuzuCurrentUser = "current_user";

constexpr std::string_view ryujinxUserSaves = "user/save";
constexpr std::string_view ryujinxProfiles = "system/Profiles.json";
/// LibHac's SaveDataExtraData: the SaveDataAttribute first, 0x200 bytes in all.
constexpr std::string_view extraData = "ExtraData0";
constexpr std::uintmax_t extraDataSize = 0x200;
constexpr std::size_t attributeProgramId = 0x00;
constexpr std::size_t attributeUserHigh = 0x08;
constexpr std::size_t attributeUserLow = 0x10;
constexpr std::size_t attributeStaticId = 0x18;
constexpr std::size_t attributeType = 0x20;
constexpr std::size_t attributeRank = 0x21;
constexpr std::size_t attributeIndex = 0x22;
/// SaveDataType: Account 1, Device 3.
constexpr std::uint8_t saveTypeAccount = 1;
constexpr std::uint8_t saveTypeDevice = 3;
/// DirectorySaveDataFileSystem's committed and working folders.
constexpr std::string_view committed = "0";
constexpr std::string_view working = "1";
constexpr std::string_view extraDataCopy = "ExtraData1";
constexpr std::string_view saveLock = ".lock";
/// SaveDataExtraData after the attribute: owner, time stamp, flags and format, sizes, commit id.
constexpr std::size_t extraOwner = 0x40;
constexpr std::size_t extraDataSizeAt = 0x58;
constexpr std::size_t extraJournalSize = 0x60;
constexpr std::size_t extraCommitId = 0x68;
/// Kenji-NX holds this beside `user/save` while it runs; its saves are filed by game and profile.
constexpr std::string_view kenjiLayoutLock = "user/save-layout.lock";
constexpr std::string_view kenjiDeviceFolder = "device";
/// Ryubing's save index: the system save 8000000000000000, a KeyValueArchive of
/// SaveDataAttribute to SaveDataIndexerValue, and the last save id it gave.
constexpr std::string_view ryubingIndexSave = "system/save/8000000000000000";
constexpr std::string_view ryubingIndex = "imkvdb.arc";
constexpr std::string_view ryubingLastId = "lastPublishedId";
constexpr std::string_view archiveMagic = "IMKV";
constexpr std::string_view entryMagic = "IMEN";
constexpr std::size_t archiveHeader = 12;
constexpr std::size_t entryHeader = 12;
constexpr std::size_t attributeSize = 0x40;
constexpr std::size_t indexValueSize = 0x40;
/// SaveDataIndexerValue: the save id first, the space id at 0x18 (1 is the user partition).
constexpr std::size_t indexValueSpace = 0x18;
constexpr std::uint8_t userSpace = 1;

std::string readBytes(const fs::path& file) {
    std::ifstream in{file, std::ios::binary};
    return {std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}};
}

using fileio::littleEndian;
using fileio::putLittleEndian;

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

/// A UUID's 16 bytes as yuzu names its user folder: the second 64 bits then the first, each
/// little-endian and upper-case hex.
std::string yuzuUserName(std::string_view uuid) {
    const std::uint64_t first = littleEndian(uuid, 0, sizeof(std::uint64_t));
    const std::uint64_t second = littleEndian(uuid, sizeof(std::uint64_t), sizeof(std::uint64_t));
    return hex16(second) + hex16(first);
}

/// A UUID's 16 bytes from Eden's newer folder name, its bytes in order as lower-case hex; nothing
/// for another name.
std::optional<std::string> edenUuid(std::string_view name) {
    if (name.size() != 2 * uuidSize) {
        return std::nullopt;
    }
    std::string bytes(uuidSize, '\0');
    for (std::size_t i = 0; i < uuidSize; ++i) {
        unsigned value = 0;
        const char* at = name.data() + (2 * i);
        const auto [end, error] = std::from_chars(at, at + 2, value, 16);
        if (error != std::errc{} || end != at + 2) {
            return std::nullopt;
        }
        bytes[i] = static_cast<char>(value);
    }
    return bytes;
}

/// The user folders of profiles.dat in its order, as yuzu names them (`yuzuUserName`).
std::vector<std::string> yuzuUsers(const fs::path& nand) {
    const std::string bytes = readBytes(nand / yuzuProfiles);
    std::vector<std::string> users;
    for (std::size_t i = 0; i < profileCount; ++i) {
        const std::size_t at = profilesHeader + (i * profileSize);
        if (bytes.size() < at + uuidSize) {
            break;
        }
        const std::string_view uuid = std::string_view{bytes}.substr(at, uuidSize);
        if (uuid.find_first_not_of('\0') != std::string_view::npos) {
            users.push_back(yuzuUserName(uuid));
        }
    }
    return users;
}

/// qt-config.ini's `current_user`, an index into the profiles; 0 when it names none.
std::size_t yuzuCurrentIndex(const fs::path& config) {
    const std::string value =
        iniValue(config / yuzuSettingsFile, "System", yuzuCurrentUser).value_or("");
    std::size_t index = 0;
    const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), index);
    return error == std::errc{} ? index : 0;
}

/// An emulator's users in its own order, and the one it has open; empty when it has none.
struct Users {
    std::vector<std::string> all;
    std::string current;

    /// The user `seat` plays as: the first seat the current user, each further one the next of
    /// the others; empty when there are not that many.
    [[nodiscard]] std::string ofSeat(std::size_t seat) const {
        if (seat == 0) {
            return current;
        }
        std::size_t left = seat;
        for (const std::string& user : all) {
            if (user != current && --left == 0) {
                return user;
            }
        }
        return {};
    }
};

Users yuzuUsersOf(const fs::path& nand, const fs::path& config) {
    Users users{.all = yuzuUsers(nand), .current = {}};
    if (const std::size_t index = yuzuCurrentIndex(config); index < users.all.size()) {
        users.current = users.all[index];
    }
    return users;
}

/// Profiles.json's users, upper-case, with the open one current.
Users ryujinxUsersOf(const fs::path& config) {
    const nlohmann::json root =
        nlohmann::json::parse(readBytes(config / ryujinxProfiles), nullptr, false);
    Users users;
    if (root.is_discarded() || !root.is_object() || !root.contains("profiles") ||
        !root["profiles"].is_array()) {
        return users;
    }
    for (const nlohmann::json& profile : root["profiles"]) {
        if (!profile.is_object() || !profile.contains("user_id") ||
            !profile["user_id"].is_string()) {
            continue;
        }
        std::string user = profile["user_id"].get<std::string>();
        std::ranges::transform(user, user.begin(), [](unsigned char c) {
            return static_cast<char>(std::toupper(c));
        });
        if (users.current.empty() && profile.value("account_state", "") == "Open") {
            users.current = user;
        }
        users.all.push_back(std::move(user));
    }
    return users;
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

/// The user a new account save is made for, as the emulator spells it; empty when it has none for
/// the seat.
std::string seatUser(const SaveHome& home) {
    const Users users = home.layout == roms::SaveLayout::RyujinxBis
                            ? ryujinxUsersOf(home.config)
                            : yuzuUsersOf(home.folder, home.config);
    return users.ofSeat(home.seat);
}

/// A user id spelled as 32 hex digits, its high 64 bits first.
std::optional<std::pair<std::uint64_t, std::uint64_t>> userHalves(std::string_view user) {
    if (user.size() != 2 * sizeof(std::uint64_t) * 2) {
        return std::nullopt;
    }
    const std::optional<std::uint64_t> high = titleNumber(user.substr(0, user.size() / 2));
    const std::optional<std::uint64_t> low = titleNumber(user.substr(user.size() / 2));
    if (!high || !low) {
        return std::nullopt;
    }
    return std::pair{*high, *low};
}

/// The SaveDataAttribute of `title`'s primary `kind` save for `user` (empty for a device save).
std::string attributeOf(std::uint64_t title, SlotKind kind, std::string_view user) {
    std::string bytes(attributeSize, '\0');
    putLittleEndian(bytes, attributeProgramId, title, sizeof(std::uint64_t));
    if (const auto halves = userHalves(user); kind == SlotKind::Account && halves) {
        putLittleEndian(bytes, attributeUserHigh, halves->first, sizeof(std::uint64_t));
        putLittleEndian(bytes, attributeUserLow, halves->second, sizeof(std::uint64_t));
    }
    putLittleEndian(bytes, attributeType,
                    kind == SlotKind::Account ? saveTypeAccount : saveTypeDevice, 1);
    return bytes;
}

/// LibHac's SaveDataAttribute order: program, type, user, static save id, index, rank.
bool attributeBefore(std::string_view a, std::string_view b) {
    const auto fields = [](std::string_view bytes) {
        return std::tuple{littleEndian(bytes, attributeProgramId, sizeof(std::uint64_t)),
                          littleEndian(bytes, attributeType, 1),
                          littleEndian(bytes, attributeUserHigh, sizeof(std::uint64_t)),
                          littleEndian(bytes, attributeUserLow, sizeof(std::uint64_t)),
                          littleEndian(bytes, attributeStaticId, sizeof(std::uint64_t)),
                          littleEndian(bytes, attributeIndex, 2),
                          littleEndian(bytes, attributeRank, 1)};
    };
    return fields(a) < fields(b);
}

/// The ExtraData0 and ExtraData1 Ryujinx writes for a new save: the attribute, the owner, the
/// sizes control.nacp gives the kind, a fresh commit id.
std::string extraDataOf(const SaveHome& home, std::uint64_t title, SlotKind kind,
                        std::string_view user) {
    std::string bytes(extraDataSize, '\0');
    const std::string attribute = attributeOf(title, kind, user);
    bytes.replace(0, attribute.size(), attribute);
    const SwitchSaveData data = home.saveData.value_or(SwitchSaveData{});
    putLittleEndian(bytes, extraOwner, data.owner != 0 ? data.owner : title, sizeof(std::uint64_t));
    const bool account = kind == SlotKind::Account;
    putLittleEndian(bytes, extraDataSizeAt, account ? data.accountSize : data.deviceSize,
                    sizeof(std::uint64_t));
    putLittleEndian(bytes, extraJournalSize, account ? data.accountJournal : data.deviceJournal,
                    sizeof(std::uint64_t));
    std::random_device random;
    const std::uint64_t commit = (static_cast<std::uint64_t>(random()) << 32U) | random();
    putLittleEndian(bytes, extraCommitId, commit, sizeof(std::uint64_t));
    return bytes;
}

void writeFile(const fs::path& file, std::string_view bytes) {
    std::string error;
    if (!fileio::writeWhole(file, bytes, error)) {
        throw fs::filesystem_error{error, file, std::make_error_code(std::errc::io_error)};
    }
}

/// One KeyValueArchive entry: its key and value bytes.
struct ArchiveEntry {
    std::string key;
    std::string value;
};

/// The entries of a KeyValueArchive; nothing when it is not one.
std::optional<std::vector<ArchiveEntry>> archiveEntries(std::string_view bytes) {
    if (bytes.size() < archiveHeader || bytes.substr(0, archiveMagic.size()) != archiveMagic) {
        return std::nullopt;
    }
    const std::uint64_t count = littleEndian(bytes, 8, 4);
    std::vector<ArchiveEntry> entries;
    std::size_t at = archiveHeader;
    for (std::uint64_t i = 0; i < count; ++i) {
        if (bytes.size() < at + entryHeader || bytes.substr(at, entryMagic.size()) != entryMagic) {
            return std::nullopt;
        }
        const std::size_t keySize = littleEndian(bytes, at + 4, 4);
        const std::size_t valueSize = littleEndian(bytes, at + 8, 4);
        at += entryHeader;
        if (bytes.size() < at + keySize + valueSize) {
            return std::nullopt;
        }
        entries.push_back({std::string{bytes.substr(at, keySize)},
                           std::string{bytes.substr(at + keySize, valueSize)}});
        at += keySize + valueSize;
    }
    return entries;
}

std::string archiveBytes(const std::vector<ArchiveEntry>& entries) {
    std::string bytes{archiveMagic};
    bytes.resize(archiveHeader, '\0');
    putLittleEndian(bytes, 8, entries.size(), 4);
    for (const ArchiveEntry& entry : entries) {
        std::string header{entryMagic};
        header.resize(entryHeader, '\0');
        putLittleEndian(header, 4, entry.key.size(), 4);
        putLittleEndian(header, 8, entry.value.size(), 4);
        bytes += header + entry.key + entry.value;
    }
    return bytes;
}

bool kenjiLayout(const fs::path& bis) {
    std::error_code ec;
    return fs::exists(bis / kenjiLayoutLock, ec);
}

/// Ryubing's index and the next save id it would give; nothing when Ryujinx has not made it yet.
struct RyubingIndex {
    std::vector<ArchiveEntry> entries;
    std::uint64_t nextId{0};
};

/// The index, its next save id no lower than `atLeast`.
std::optional<RyubingIndex> ryubingIndexOf(const fs::path& bis, std::uint64_t atLeast) {
    const fs::path folder = bis / ryubingIndexSave / committed;
    std::optional<std::vector<ArchiveEntry>> entries =
        archiveEntries(readBytes(folder / ryubingIndex));
    if (!entries) {
        return std::nullopt;
    }
    // The indexer publishes the id after the last it gave; system saves keep their static ids.
    const std::string last = readBytes(folder / ryubingLastId);
    std::uint64_t next = std::max(
        atLeast,
        (last.size() == sizeof(std::uint64_t) ? littleEndian(last, 0, sizeof(std::uint64_t)) : 0) +
            1);
    const auto taken = [&entries](std::uint64_t id) {
        return std::ranges::any_of(*entries, [id](const ArchiveEntry& entry) {
            return entry.value.size() >= sizeof(std::uint64_t) &&
                   littleEndian(entry.value, 0, sizeof(std::uint64_t)) == id;
        });
    };
    while (taken(next)) {
        ++next;
    }
    return RyubingIndex{.entries = std::move(*entries), .nextId = next};
}

} // namespace

std::vector<SaveSlot> yuzuSlots(const fs::path& nand, const fs::path& config, std::size_t seat,
                                std::string_view titleId) {
    const std::string current = yuzuUsersOf(nand, config).ofSeat(seat);
    // Each user's save of the title by its user folder's name; Eden's newer place wins.
    std::map<std::string, std::pair<fs::path, fs::path>> saves;
    const std::string title{titleId};
    std::error_code ec;
    for (const fs::path& user : folders(nand / yuzuUserSaves)) {
        if (const fs::path save = user / title; fs::is_directory(save, ec)) {
            saves[user.filename().string()] = {save, save};
        }
    }
    for (const fs::path& user : folders(nand / edenAccountSaves)) {
        const std::optional<std::string> uuid = edenUuid(user.filename().string());
        if (const fs::path save = user / title;
            uuid && fs::is_directory(save / edenSaveFolder, ec)) {
            saves[yuzuUserName(*uuid)] = {save, save / edenSaveFolder};
        }
    }
    if (const fs::path save = nand / edenDeviceSaves / title;
        fs::is_directory(save / edenSaveFolder, ec)) {
        saves[std::string{yuzuDeviceUser}] = {save, save / edenSaveFolder};
    }
    std::vector<SaveSlot> slots;
    for (const auto& [name, where] : saves) {
        SaveSlot slot;
        slot.kind = name == yuzuDeviceUser ? SlotKind::Device : SlotKind::Account;
        if (slot.kind == SlotKind::Account) {
            slot.user = name;
            slot.current = !current.empty() && name == current;
        }
        slot.root = where.first;
        slot.content = {where.second};
        slots.push_back(std::move(slot));
    }
    return slots;
}

std::vector<SaveSlot> ryujinxSlots(const fs::path& bis, const fs::path& config, std::size_t seat,
                                   std::string_view titleId) {
    const std::optional<std::uint64_t> title = titleNumber(titleId);
    if (!title) {
        lucent::warn("saves", "{} is not a Switch title id", titleId);
        return {};
    }
    const std::string current = ryujinxUsersOf(config).ofSeat(seat);
    std::vector<SaveSlot> slots;
    const auto add = [&](const fs::path& folder) {
        if (std::optional<SaveSlot> slot = ryujinxSlot(folder, *title)) {
            slot->current =
                slot->kind == SlotKind::Account && !current.empty() && slot->user == current;
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

std::optional<NewSlot> plannedSlot(const SaveHome& home, SlotKind kind,
                                   const std::vector<NewSlot>& earlier, std::string& error) {
    if (kind != SlotKind::Account && kind != SlotKind::Device) {
        error = "only a Switch save can be made here";
        return std::nullopt;
    }
    const std::optional<std::uint64_t> title = titleNumber(home.titleId);
    if (!title) {
        error = home.titleId + " is not a Switch title id";
        return std::nullopt;
    }
    NewSlot planned;
    planned.slot.kind = kind;
    if (kind == SlotKind::Account) {
        planned.slot.user = seatUser(home);
        planned.slot.current = true;
        if (planned.slot.user.empty()) {
            error = home.seat == 0 ? "it has no user profile yet: start it once"
                                   : "it has no user for this profile: add a user in it";
            return std::nullopt;
        }
    }
    if (home.layout == roms::SaveLayout::YuzuNand) {
        const std::string user =
            kind == SlotKind::Account ? planned.slot.user : std::string{yuzuDeviceUser};
        planned.slot.root = home.folder / yuzuUserSaves / user / home.titleId;
        planned.slot.content = {planned.slot.root};
        return planned;
    }
    if (!home.saveData) {
        error = "the game's save sizes are not known yet";
        return std::nullopt;
    }
    if (kenjiLayout(home.folder)) {
        planned.slot.root =
            home.folder / ryujinxUserSaves / home.titleId /
            (kind == SlotKind::Account ? planned.slot.user : std::string{kenjiDeviceFolder});
    } else {
        std::uint64_t after = 0;
        for (const NewSlot& before : earlier) {
            after = std::max(after, titleNumber(before.slot.root.filename().string()).value_or(0));
        }
        const std::optional<RyubingIndex> index = ryubingIndexOf(home.folder, after + 1);
        if (!index) {
            error = "it has no save index yet: start it once";
            return std::nullopt;
        }
        planned.slot.root = home.folder / ryujinxUserSaves / hex16(index->nextId);
        planned.touched = {home.folder / ryubingIndexSave};
    }
    planned.slot.content = {planned.slot.root / committed, planned.slot.root / working};
    return planned;
}

void makeSlot(const SaveHome& home, const NewSlot& planned) {
    const SaveSlot& slot = planned.slot;
    fs::create_directories(slot.root);
    if (home.layout == roms::SaveLayout::YuzuNand) {
        return;
    }
    const std::uint64_t title = titleNumber(home.titleId).value_or(0);
    const std::string extra = extraDataOf(home, title, slot.kind, slot.user);
    for (const fs::path& folder : slot.content) {
        fs::create_directories(folder);
    }
    writeFile(slot.root / extraData, extra);
    writeFile(slot.root / extraDataCopy, extra);
    writeFile(slot.root / saveLock, {});
    if (kenjiLayout(home.folder)) {
        return;
    }
    // Ryubing finds a save through its index entry, which names the folder by the save id.
    std::optional<RyubingIndex> index = ryubingIndexOf(home.folder, 0);
    const std::optional<std::uint64_t> id = titleNumber(slot.root.filename().string());
    if (!index || !id) {
        throw fs::filesystem_error{"Ryujinx's save index cannot be read",
                                   home.folder / ryubingIndexSave,
                                   std::make_error_code(std::errc::invalid_argument)};
    }
    ArchiveEntry entry{.key = attributeOf(title, slot.kind, slot.user),
                       .value = std::string(indexValueSize, '\0')};
    putLittleEndian(entry.value, 0, *id, sizeof(std::uint64_t));
    putLittleEndian(entry.value, indexValueSpace, userSpace, 1);
    const auto at = std::ranges::find_if(index->entries, [&entry](const ArchiveEntry& other) {
        return !attributeBefore(other.key, entry.key);
    });
    if (at != index->entries.end() && at->key == entry.key) {
        at->value = entry.value;
    } else {
        index->entries.insert(at, entry);
    }
    std::string last(sizeof(std::uint64_t), '\0');
    putLittleEndian(last, 0, *id, sizeof(std::uint64_t));
    for (const std::string_view side : {committed, working}) {
        const fs::path folder = home.folder / ryubingIndexSave / side;
        writeFile(folder / ryubingIndex, archiveBytes(index->entries));
        writeFile(folder / ryubingLastId, last);
    }
}

} // namespace opensu::library::saves
