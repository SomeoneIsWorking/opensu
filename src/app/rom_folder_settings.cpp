#include "rom_folder_settings.hpp"

#include <algorithm>
#include <memory>
#include <utility>

#include "gpu2d/ui/screen.hpp"

namespace opensu::app {
namespace {

namespace fs = std::filesystem;
using ui::RowKind;

constexpr std::string_view removePrefix = "roms.remove.";
constexpr std::string_view startedPrefix = "roms.started.";
constexpr std::string_view findId = "roms.reset";

} // namespace

std::vector<ui::SettingsRow> RomFolderSettings::rows() const {
    const settings::Settings& v = preferences_.values();
    std::vector<ui::SettingsRow> rows{ui::makeHeading("ROM folders")};
    for (const fs::path& folder : v.romFolders) {
        rows.push_back(ui::makeRow(std::string{removePrefix} + folder.string(), RowKind::Action,
                                   tree_->label(folder), "The library reads this folder",
                                   "Remove"));
    }
    if (v.romFolders.empty()) {
        for (const fs::path& folder : defaults_.romRoots) {
            rows.push_back(ui::makeRow(std::string{startedPrefix} + folder.string(), RowKind::Info,
                                       tree_->label(folder), std::string{ui::startedWith}, ""));
        }
    }
    if (v.romFolders.empty() && defaults_.romRoots.empty()) {
        rows.push_back(ui::makeRow("roms.list", RowKind::Info, "ROM folders",
                                   "The library reads these",
                                   v.findRomFolders ? "Found automatically" : "None"));
    }
    rows.push_back(ui::makeRow(std::string{addId}, RowKind::Folder, "Add a ROM folder",
                               "A folder with one subfolder per system", ""));
    rows.push_back(ui::makeRow(std::string{findId}, RowKind::Action,
                               "Find ROM folders automatically", "Forgets the folders you added",
                               ""));
    return rows;
}

bool RomFolderSettings::owns(const std::string& id) {
    return id == findId || id.starts_with(removePrefix);
}

void RomFolderSettings::run(const std::string& id, int step) {
    if (step != 0) {
        return;
    }
    if (id == findId) {
        findAutomatically();
        return;
    }
    ask(id.substr(removePrefix.size()));
}

void RomFolderSettings::ask(const fs::path& folder) {
    const std::vector<fs::path>& folders = preferences_.values().romFolders;
    if (std::ranges::find(folders, folder) == folders.end()) {
        return;
    }
    std::string detail = tree_->label(folder) +
                         " is forgotten and no file changes. Its games leave Library; adding it "
                         "again brings them back with their saves and settings.";
    if (folders.size() == 1 && defaults_.romRoots.empty()) {
        detail += " No ROM folder is left, until you add one or find them automatically.";
    }
    sounds_.play(audio::Effect::Open);
    navigator_.push(
        gpu2d::ui::confirm("Remove this ROM folder?", std::move(detail), "Remove",
                           [this, forgotten = std::make_shared<const fs::path>(folder)] {
                               remove(*forgotten);
                           }));
}

void RomFolderSettings::remove(const fs::path& folder) {
    preferences_.values().removeRomFolder(folder);
    hooks_.changed();
    release(folder);
    hooks_.say("ROM folder removed");
}

void RomFolderSettings::findAutomatically() {
    settings::Settings& v = preferences_.values();
    const std::vector<fs::path> forgotten = std::exchange(v.romFolders, {});
    v.findRomFolders = true;
    hooks_.changed();
    for (const fs::path& folder : forgotten) {
        release(folder);
    }
    hooks_.say("ROM folders are found automatically");
}

void RomFolderSettings::release(const fs::path& folder) const {
    if (!preferences_.values().namesFolder(folder)) {
        tree_->release(folder);
    }
}

} // namespace opensu::app
