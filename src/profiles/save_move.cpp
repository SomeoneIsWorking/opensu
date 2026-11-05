#include "save_move.hpp"

#include <algorithm>
#include <initializer_list>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "library/save_copy.hpp"
#include "lucent/log.h"
#include "retro/save_paths.hpp"
#include "save_sync/save_versions.hpp"

namespace opensu::profiles {
namespace {

namespace fs = std::filesystem;

constexpr std::string_view partSuffix = ".part";

/// What `root` holds besides the folders named `apart`.
std::vector<fs::path> earlierEntries(const fs::path& root,
                                     std::initializer_list<std::string_view> apart) {
    std::vector<fs::path> found;
    std::error_code ec;
    for (const fs::directory_entry& entry : fs::directory_iterator{root, ec}) {
        const std::string name = entry.path().filename().string();
        if (std::ranges::find(apart, name) == apart.end()) {
            found.push_back(entry.path());
        }
    }
    std::ranges::sort(found);
    return found;
}

/// The files under `entries` (each under `root`) with no file at their place under `into`, and
/// every folder among them; a file whose place is taken is counted in `moved` and stays.
struct Plan {
    std::vector<fs::path> files;
    std::vector<fs::path> folders;
};

Plan planMove(const std::vector<fs::path>& entries, const fs::path& root, const fs::path& into,
              Moved& moved) {
    Plan plan;
    std::vector<fs::path> found;
    for (const fs::path& entry : entries) {
        std::error_code ec;
        if (!fs::is_directory(fs::symlink_status(entry, ec))) {
            found.push_back(entry);
            continue;
        }
        plan.folders.push_back(entry);
        for (fs::recursive_directory_iterator it{entry, ec}, end; it != end; it.increment(ec)) {
            (it->is_directory(ec) && !it->is_symlink(ec) ? plan.folders : found)
                .push_back(it->path());
        }
    }
    for (fs::path& file : found) {
        const fs::path to = into / file.lexically_relative(root);
        std::error_code ec;
        if (fs::exists(fs::symlink_status(to, ec))) {
            ++moved.left;
            lucent::warn("profiles", "{} stays where it is: {} is there already", file.string(),
                         to.string());
            continue;
        }
        plan.files.push_back(std::move(file));
    }
    return plan;
}

/// A copy of `files` (each under `root`) in a new folder under `parent`, made beside it and
/// renamed when whole; a copy an earlier run left unfinished goes first.
fs::path backUp(const std::vector<fs::path>& files, const fs::path& root, const fs::path& parent,
                std::chrono::system_clock::time_point now) {
    std::error_code ec;
    for (const fs::directory_entry& entry : fs::directory_iterator{parent, ec}) {
        if (entry.path().filename().string().ends_with(partSuffix)) {
            fs::remove_all(entry.path());
        }
    }
    const fs::path whole = library::saves::newBackupFolder(parent, now);
    fs::path part = whole;
    part += partSuffix;
    for (const fs::path& file : files) {
        const fs::path to = part / file.lexically_relative(root);
        fs::create_directories(to.parent_path());
        fs::copy(file, to, fs::copy_options::copy_symlinks);
    }
    fs::rename(part, whole);
    return whole;
}

/// Renames each of `plan`'s files, under `root`, to the same place under `into`; then the
/// folders left empty go.
void moveInto(Plan plan, const fs::path& root, const fs::path& into, Moved& moved) {
    for (const fs::path& file : plan.files) {
        const fs::path to = into / file.lexically_relative(root);
        fs::create_directories(to.parent_path());
        fs::rename(file, to);
        ++moved.files;
    }
    // Deepest first, so a folder is empty by the time it is reached.
    std::ranges::sort(plan.folders, std::greater{});
    for (const fs::path& folder : plan.folders) {
        std::error_code ec;
        fs::remove(folder, ec);
    }
}

} // namespace

Moved moveIntoProfile(const SaveMove& move, std::string_view profileId,
                      std::chrono::system_clock::time_point now) {
    Moved moved;
    const fs::path into = retro::profileSaveFolder(move.saves, profileId);
    Plan saves =
        planMove(earlierEntries(move.saves, {retro::profilesFolder}), move.saves, into, moved);
    const fs::path backups = move.state / library::saves::backupsFolder;
    if (!saves.files.empty()) {
        moved.backup = backUp(saves.files, move.saves, backups / beforeProfilesFolder, now);
    }
    moveInto(std::move(saves), move.saves, into, moved);
    const fs::path keptInto = library::saves::profileBackups(move.state, profileId);
    moveInto(planMove(earlierEntries(backups,
                                     {library::saves::profileBackupsFolder, beforeProfilesFolder}),
                      backups, keptInto, moved),
             backups, keptInto, moved);
    save_sync::SaveVersions{move.versions}.adopt(profileId);
    if (moved.files + moved.left == 0) {
        return moved;
    }
    lucent::info("profiles",
                 "moved {} save files into profile {} ({} left where they were), backup: {}",
                 moved.files, profileId, moved.left,
                 moved.backup.empty() ? std::string{"none"} : moved.backup.string());
    return moved;
}

} // namespace opensu::profiles
