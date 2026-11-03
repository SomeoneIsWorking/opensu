#include "save_copy.hpp"

#include <array>
#include <format>
#include <fstream>
#include <iterator>
#include <system_error>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "fileio/atomic_write.hpp"
#include "lucent/log.h"

namespace opensu::library::saves {
namespace {

namespace fs = std::filesystem;
using json = nlohmann::json;
using TimePoint = std::chrono::system_clock::time_point;

constexpr std::string_view backupsFolder = "save-backups";
constexpr std::string_view manifestFile = "backup.json";
constexpr std::array kinds{SlotKind::Account, SlotKind::Device, SlotKind::Memory, SlotKind::State};

/// A slot copied over another.
struct Pair {
    const SaveSlot* from;
    const SaveSlot* to;
};

/// One slot of a backup: where it was and its copy's folder in the backup.
struct Kept {
    fs::path root;
    fs::path copy;
};

/// Copies `from`, a file or folder, to `to`, which is not there yet, keeping each file's bytes and
/// modification time.
void copyTree(const fs::path& from, const fs::path& to) {
    if (fs::is_symlink(from)) {
        fs::copy_symlink(from, to);
        return;
    }
    if (fs::is_regular_file(from)) {
        fs::copy_file(from, to);
        fs::last_write_time(to, fs::last_write_time(from));
        return;
    }
    fs::create_directory(to);
    for (const fs::directory_entry& entry : fs::directory_iterator{from}) {
        copyTree(entry.path(), to / entry.path().filename());
    }
    fs::last_write_time(to, fs::last_write_time(from));
}

/// `target` made what `source` is: a copy of it, or an empty folder when it is not there.
void replaceWith(const fs::path& source, const fs::path& target) {
    fs::remove_all(target);
    if (fs::exists(source)) {
        copyTree(source, target);
    } else {
        fs::create_directories(target);
    }
}

std::string readText(const fs::path& file) {
    std::ifstream in{file, std::ios::binary};
    return {std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}};
}

/// A backup's slots and what its manifest says, or nothing when it cannot be read.
std::optional<std::pair<SaveBackup, std::vector<Kept>>> readBackup(const fs::path& folder) {
    const json manifest = json::parse(readText(folder / manifestFile), nullptr, false);
    if (manifest.is_discarded() || !manifest.is_object() || !manifest.contains("slots") ||
        !manifest["slots"].is_array() || !manifest.contains("made") ||
        !manifest["made"].is_number_integer()) {
        return std::nullopt;
    }
    SaveBackup backup{.folder = folder,
                      .made = TimePoint{std::chrono::seconds{manifest["made"].get<long long>()}},
                      .from = manifest.value("from", "")};
    std::vector<Kept> slots;
    for (const json& slot : manifest["slots"]) {
        if (!slot.is_object() || !slot.contains("root") || !slot["root"].is_string() ||
            !slot.contains("copy") || !slot["copy"].is_string()) {
            return std::nullopt;
        }
        slots.push_back(Kept{fs::path{slot["root"].get<std::string>()},
                             folder / slot["copy"].get<std::string>()});
    }
    return std::pair{std::move(backup), std::move(slots)};
}

/// Puts each kept slot back where it was.
void restore(const std::vector<Kept>& slots) {
    for (const Kept& slot : slots) {
        fs::remove_all(slot.root);
        copyTree(slot.copy, slot.root);
    }
}

/// A folder for a new backup under `parent`, named for `now`.
fs::path newBackupFolder(const fs::path& parent, TimePoint now) {
    const std::string stamp =
        std::format("{:%Y%m%dT%H%M%SZ}", std::chrono::floor<std::chrono::seconds>(now));
    fs::path folder = parent / stamp;
    for (int again = 2; fs::exists(folder); ++again) {
        folder = parent / (stamp + "-" + std::to_string(again));
    }
    return folder;
}

} // namespace

fs::path SaveCopier::folderOf(const SaveKey& key, std::string_view emulatorId) const {
    return backups_ / backupsFolder / key.system / key.game / std::string{emulatorId};
}

std::string SaveCopier::copy(const SaveKey& key, const EmulatorSaves& from, const EmulatorSaves& to,
                             TimePoint now) const {
    std::vector<Pair> pairs;
    bool anyFiles = false;
    for (const SlotKind kind : kinds) {
        const SaveSlot* source = from.slotFor(kind);
        const SaveSlot* target = to.slotFor(kind);
        if (source != nullptr && target != nullptr) {
            pairs.push_back(Pair{source, target});
            anyFiles = anyFiles || source->modified.has_value();
        }
    }
    if (pairs.empty()) {
        return from.emulatorName + " and " + to.emulatorName +
               " keep no save of the same kind for this game";
    }
    if (!anyFiles) {
        return from.emulatorName + "'s save of this game holds no files";
    }
    const fs::path folder = newBackupFolder(folderOf(key, to.emulatorId), now);
    std::vector<Kept> kept;
    json slots = json::array();
    try {
        fs::create_directories(folder);
        for (const Pair& pair : pairs) {
            const std::string name = std::to_string(kept.size());
            copyTree(pair.to->root, folder / name);
            kept.push_back(Kept{pair.to->root, folder / name});
            slots.push_back({{"root", pair.to->root.string()}, {"copy", name}});
        }
    } catch (const fs::filesystem_error& failure) {
        std::error_code ec;
        fs::remove_all(folder, ec);
        return "Could not back up " + to.emulatorName + "'s saves: " + failure.what();
    }
    const json manifest{
        {"from", from.emulatorName},
        {"made", std::chrono::floor<std::chrono::seconds>(now).time_since_epoch().count()},
        {"slots", std::move(slots)}};
    if (std::string error; !fileio::writeWhole(folder / manifestFile, manifest.dump(2), error)) {
        std::error_code ec;
        fs::remove_all(folder, ec);
        return "Could not back up " + to.emulatorName + "'s saves: " + error;
    }
    try {
        for (const Pair& pair : pairs) {
            for (const fs::path& target : pair.to->content) {
                replaceWith(pair.from->content.front(), target);
            }
        }
    } catch (const fs::filesystem_error& failure) {
        try {
            restore(kept);
        } catch (const fs::filesystem_error& again) {
            lucent::error("saves", "{} is kept: putting it back failed: {}", folder.string(),
                          again.what());
        }
        return "Could not copy the saves: " + std::string{failure.what()};
    }
    lucent::info("saves", "{} {}: {}'s saves copied over {}'s, backed up in {}", key.system,
                 key.game, from.emulatorName, to.emulatorName, folder.string());
    return {};
}

std::optional<SaveBackup> SaveCopier::latest(const SaveKey& key,
                                             std::string_view emulatorId) const {
    std::optional<SaveBackup> newest;
    std::error_code ec;
    for (const fs::directory_entry& entry : fs::directory_iterator{folderOf(key, emulatorId), ec}) {
        if (!entry.is_directory(ec)) {
            continue;
        }
        if (auto read = readBackup(entry.path());
            read && (!newest || read->first.made > newest->made ||
                     (read->first.made == newest->made && entry.path() > newest->folder))) {
            newest = std::move(read->first);
        }
    }
    return newest;
}

std::string SaveCopier::undo(const SaveKey& key, std::string_view emulatorId) const {
    const std::optional<SaveBackup> newest = latest(key, emulatorId);
    if (!newest) {
        return "There is no backup to put back";
    }
    const auto read = readBackup(newest->folder);
    if (!read) {
        return newest->folder.string() + " cannot be read";
    }
    try {
        restore(read->second);
        fs::remove_all(newest->folder);
    } catch (const fs::filesystem_error& failure) {
        return "Could not put the backup back: " + std::string{failure.what()};
    }
    lucent::info("saves", "{} {}: the {} saves put back from {}", key.system, key.game, emulatorId,
                 newest->folder.string());
    return {};
}

} // namespace opensu::library::saves
