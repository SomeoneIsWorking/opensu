#include "save_copy.hpp"

#include <algorithm>
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
#include "switch_saves.hpp"

namespace opensu::library::saves {
namespace {

namespace fs = std::filesystem;
using json = nlohmann::json;
using TimePoint = std::chrono::system_clock::time_point;

constexpr std::string_view backupsFolder = "save-backups";
constexpr std::string_view manifestFile = "backup.json";
constexpr std::array kinds{SlotKind::Account, SlotKind::Device, SlotKind::Memory, SlotKind::State};

/// A slot copied over another, or over one made for it.
struct Pair {
    const SaveSlot* from;
    SaveSlot to;
    std::optional<NewSlot> made;
};

/// One slot of a backup: where it was and its copy's folder in the backup.
struct Kept {
    fs::path root;
    fs::path copy;
};

/// What a backup puts back: the slots kept, and what the copy made, removed.
struct Backup {
    SaveBackup about;
    std::vector<Kept> kept;
    std::vector<fs::path> made;
};

/// The bytes of the files under `root`, a file or folder.
std::uintmax_t treeBytes(const fs::path& root) {
    std::error_code ec;
    if (fs::is_regular_file(root, ec)) {
        return fs::file_size(root, ec);
    }
    std::uintmax_t total = 0;
    for (fs::recursive_directory_iterator it{root, ec}, end; it != end; it.increment(ec)) {
        if (it->is_regular_file(ec)) {
            total += it->file_size(ec);
        }
    }
    return total;
}

/// Copies `from`, a file or folder, to `to`, which is not there yet, keeping each file's bytes and
/// modification time.
void copyTree(const fs::path& from, const fs::path& to, CopyProgress& progress) {
    if (fs::is_symlink(from)) {
        fs::copy_symlink(from, to);
        return;
    }
    if (fs::is_regular_file(from)) {
        fs::copy_file(from, to);
        fs::last_write_time(to, fs::last_write_time(from));
        progress.done += fs::file_size(to);
        return;
    }
    fs::create_directory(to);
    for (const fs::directory_entry& entry : fs::directory_iterator{from}) {
        copyTree(entry.path(), to / entry.path().filename(), progress);
    }
    fs::last_write_time(to, fs::last_write_time(from));
}

/// `target` made what `source` is: a copy of it, or an empty folder when it is not there.
void replaceWith(const fs::path& source, const fs::path& target, CopyProgress& progress) {
    fs::remove_all(target);
    fs::create_directories(target.parent_path());
    if (fs::exists(source)) {
        copyTree(source, target, progress);
    } else {
        fs::create_directories(target);
    }
}

/// The outermost folder of `root` that is not there yet, which making it creates.
fs::path outermostMissing(const fs::path& root) {
    fs::path missing = root;
    for (fs::path up = root.parent_path(); !up.empty() && up != up.parent_path() && !fs::exists(up);
         up = up.parent_path()) {
        missing = up;
    }
    return missing;
}

std::string readText(const fs::path& file) {
    std::ifstream in{file, std::ios::binary};
    return {std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}};
}

bool isText(const json& object, std::string_view key) {
    const auto found = object.find(key);
    return found != object.end() && found->is_string();
}

/// A backup as its manifest says, or nothing when it cannot be read.
std::optional<Backup> readBackup(const fs::path& folder) {
    const json manifest = json::parse(readText(folder / manifestFile), nullptr, false);
    if (manifest.is_discarded() || !manifest.is_object() || !manifest.contains("slots") ||
        !manifest["slots"].is_array() || !manifest.contains("made") ||
        !manifest["made"].is_number_integer()) {
        return std::nullopt;
    }
    Backup backup{
        .about =
            SaveBackup{.folder = folder,
                       .made = TimePoint{std::chrono::seconds{manifest["made"].get<long long>()}},
                       .from = manifest.value("from", "")},
        .kept = {},
        .made = {}};
    for (const json& slot : manifest["slots"]) {
        if (!slot.is_object() || !isText(slot, "root") || !isText(slot, "copy")) {
            return std::nullopt;
        }
        backup.kept.push_back(Kept{fs::path{slot["root"].get<std::string>()},
                                   folder / slot["copy"].get<std::string>()});
    }
    if (const auto created = manifest.find("created"); created != manifest.end()) {
        if (!created->is_array()) {
            return std::nullopt;
        }
        for (const json& path : *created) {
            if (!path.is_string()) {
                return std::nullopt;
            }
            backup.made.emplace_back(path.get<std::string>());
        }
    }
    return backup;
}

/// Removes what the copy made and puts each kept slot back where it was.
void restore(const Backup& backup, CopyProgress& progress) {
    for (const fs::path& made : backup.made) {
        fs::remove_all(made);
    }
    for (const Kept& slot : backup.kept) {
        fs::remove_all(slot.root);
        copyTree(slot.copy, slot.root, progress);
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

/// The slots of `from` with files, each with `to`'s slot of its kind or one made for it. Empty,
/// with why in `error`, when `to` cannot take one.
std::vector<Pair> pairsOf(const EmulatorSaves& from, const EmulatorSaves& to, std::string& error) {
    std::vector<Pair> pairs;
    for (const SaveSlot* source : copiedSlots(from)) {
        const SlotKind kind = source->kind;
        const SaveSlot* target = to.slotFor(kind);
        // Another profile's save stays its own; the current profile's is made.
        if (target != nullptr &&
            (target->current || target->kind != SlotKind::Account || !to.home)) {
            pairs.push_back(Pair{.from = source, .to = *target, .made = std::nullopt});
            continue;
        }
        std::vector<NewSlot> earlier;
        for (const Pair& pair : pairs) {
            if (pair.made) {
                earlier.push_back(*pair.made);
            }
        }
        std::string why = to.emulatorName + " keeps no save of this kind";
        std::optional<NewSlot> planned =
            to.home ? plannedSlot(*to.home, kind, earlier, why) : std::nullopt;
        if (!planned) {
            error = to.emulatorName + " cannot take " + from.emulatorName + "'s save: " + why;
            return {};
        }
        pairs.push_back(Pair{.from = source, .to = planned->slot, .made = std::move(planned)});
    }
    if (pairs.empty()) {
        error = from.emulatorName + " keeps no save of this game";
    }
    return pairs;
}

} // namespace

std::vector<const SaveSlot*> copiedSlots(const EmulatorSaves& from) {
    std::vector<const SaveSlot*> slots;
    for (const SlotKind kind : kinds) {
        const SaveSlot* slot = from.slotFor(kind);
        if (slot != nullptr && slot->modified) {
            slots.push_back(slot);
        }
    }
    return slots;
}

fs::path SaveCopier::folderOf(const SaveKey& key, std::string_view emulatorId) const {
    return backups_ / backupsFolder / key.system / key.game / std::string{emulatorId};
}

std::string SaveCopier::copy(const SaveKey& key, const EmulatorSaves& from, const EmulatorSaves& to,
                             TimePoint now, CopyProgress& progress) const {
    std::string refused;
    const std::vector<Pair> pairs = pairsOf(from, to, refused);
    if (pairs.empty()) {
        return refused;
    }
    // What the backup keeps: each slot copied over, and what making a slot changes.
    std::vector<fs::path> keep;
    Backup backup;
    for (const Pair& pair : pairs) {
        // A slot the target keeps where it is not there yet, such as the runner's, is made.
        if (!pair.made && fs::exists(pair.to.root)) {
            keep.push_back(pair.to.root);
            continue;
        }
        if (!pair.made) {
            backup.made.push_back(outermostMissing(pair.to.root));
            continue;
        }
        backup.made.push_back(outermostMissing(pair.to.root));
        for (const fs::path& touched : pair.made->touched) {
            if (std::ranges::find(keep, touched) == keep.end()) {
                keep.push_back(touched);
            }
        }
    }
    std::uintmax_t total = 0;
    for (const fs::path& root : keep) {
        total += treeBytes(root);
    }
    for (const Pair& pair : pairs) {
        total += treeBytes(pair.from->content.front()) * pair.to.content.size();
    }
    progress.done = 0;
    progress.total = total;

    const fs::path folder = newBackupFolder(folderOf(key, to.emulatorId), now);
    json slots = json::array();
    try {
        fs::create_directories(folder);
        for (const fs::path& root : keep) {
            const std::string name = std::to_string(backup.kept.size());
            copyTree(root, folder / name, progress);
            backup.kept.push_back(Kept{root, folder / name});
            slots.push_back({{"root", root.string()}, {"copy", name}});
        }
    } catch (const fs::filesystem_error& failure) {
        std::error_code ec;
        fs::remove_all(folder, ec);
        return "Could not back up " + to.emulatorName + "'s saves: " + failure.what();
    }
    json made = json::array();
    for (const fs::path& path : backup.made) {
        made.push_back(path.string());
    }
    const json manifest{
        {"from", from.emulatorName},
        {"made", std::chrono::floor<std::chrono::seconds>(now).time_since_epoch().count()},
        {"slots", std::move(slots)},
        {"created", std::move(made)}};
    if (std::string error; !fileio::writeWhole(folder / manifestFile, manifest.dump(2), error)) {
        std::error_code ec;
        fs::remove_all(folder, ec);
        return "Could not back up " + to.emulatorName + "'s saves: " + error;
    }
    try {
        for (const Pair& pair : pairs) {
            if (pair.made && to.home) {
                makeSlot(*to.home, *pair.made);
            }
            for (const fs::path& target : pair.to.content) {
                replaceWith(pair.from->content.front(), target, progress);
            }
        }
    } catch (const fs::filesystem_error& failure) {
        try {
            restore(backup, progress);
            fs::remove_all(folder);
        } catch (const fs::filesystem_error& again) {
            lucent::error("saves", "{} is kept: putting it back failed: {}", folder.string(),
                          again.what());
        }
        return "Could not copy the saves: " + std::string{failure.what()};
    }
    lucent::info("saves", "{} {}: {}'s saves copied over {}'s ({} made), backed up in {}",
                 key.system, key.game, from.emulatorName, to.emulatorName, backup.made.size(),
                 folder.string());
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
        if (std::optional<Backup> read = readBackup(entry.path());
            read && (!newest || read->about.made > newest->made ||
                     (read->about.made == newest->made && entry.path() > newest->folder))) {
            newest = std::move(read->about);
        }
    }
    return newest;
}

std::string SaveCopier::undo(const SaveKey& key, std::string_view emulatorId,
                             CopyProgress& progress) const {
    const std::optional<SaveBackup> newest = latest(key, emulatorId);
    if (!newest) {
        return "There is no backup to put back";
    }
    const std::optional<Backup> backup = readBackup(newest->folder);
    if (!backup) {
        return newest->folder.string() + " cannot be read";
    }
    std::uintmax_t total = 0;
    for (const Kept& slot : backup->kept) {
        total += treeBytes(slot.copy);
    }
    progress.done = 0;
    progress.total = total;
    try {
        restore(*backup, progress);
        fs::remove_all(newest->folder);
    } catch (const fs::filesystem_error& failure) {
        return "Could not put the backup back: " + std::string{failure.what()};
    }
    lucent::info("saves", "{} {}: the {} saves put back from {}, {} made removed", key.system,
                 key.game, emulatorId, newest->folder.string(), backup->made.size());
    return {};
}

} // namespace opensu::library::saves
