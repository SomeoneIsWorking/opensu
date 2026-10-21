#include "emulator_settings.hpp"

#include <algorithm>
#include <filesystem>
#include <iterator>
#include <vector>

#include "library/core_bios.hpp"
#include "library/rom_systems.hpp"

namespace opensu::app {
namespace {

using library::roms::EmulatorEntry;
using library::roms::EmulatorInstall;
using library::roms::InstallSource;
using ui::RowKind;

constexpr std::string_view checkId = "emulators.check";
constexpr std::string_view emulatorId = "emulators.run.";
constexpr std::string_view removeId = "emulators.remove.";
constexpr std::string_view biosId = "emulators.bios.";

std::string systemsOf(const EmulatorEntry& entry) {
    std::string text;
    for (const std::string& system : entry.systems) {
        text += (text.empty() ? "" : ", ") + library::roms::systemLabel(system);
    }
    return text;
}

std::string checkValue(const EmulatorDownloads::Check& check, std::size_t updates) {
    if (check.running) {
        return "Checking…";
    }
    if (!check.done) {
        return "Not checked";
    }
    if (!check.failures.empty()) {
        return check.failures.front();
    }
    if (updates == 0) {
        return "Everything is up to date";
    }
    return std::to_string(updates) + (updates == 1 ? " update" : " updates");
}

} // namespace

bool EmulatorSettings::owns(const std::string& id) {
    return id.starts_with(std::string{categoryId} + ".");
}

void EmulatorSettings::rescan() {
    found_ = downloads_.discover();
    armed_.reset();
}

std::string EmulatorSettings::noteOf(const EmulatorEntry& entry) const {
    const EmulatorInstall* install = found_.installed(entry.id);
    std::string note;
    if (install == nullptr) {
        note = "Not installed";
    } else if (!install->problem.empty()) {
        note = install->problem;
    } else {
        note = std::string{library::roms::label(install->source)} +
               (install->version.empty() ? "" : " " + install->version);
        if (install->source == InstallSource::Core) {
            note += " in " + std::filesystem::path{install->where}.parent_path().string();
        }
    }
    note += " · " + systemsOf(entry);
    if (!entry.config.empty()) {
        note += " · settings in " + entry.config.string();
    }
    return note;
}

std::string EmulatorSettings::valueOf(const EmulatorEntry& entry) const {
    if (const std::optional<EmulatorDownloads::Progress> progress = downloads_.progress();
        progress && progress->id == entry.id) {
        const std::optional<int> percent = progress->percent();
        return "Downloading" + (percent ? " " + std::to_string(*percent) + "%" : std::string{}) +
               " · A cancels";
    }
    const EmulatorInstall* install = found_.installed(entry.id);
    if (install != nullptr && library::roms::downloadedByOpensu(install->source)) {
        const std::optional<EmulatorDownloads::Build> newest = downloads_.latest(entry.id);
        if (!newest) {
            return "Installed";
        }
        return downloads_.updateAvailable(entry.id) ? "Update to " + newest->version : "Up to date";
    }
    if (install != nullptr && install->problem.empty()) {
        return "Installed";
    }
    return entry.downloadable() ? "Download" : "Install it yourself";
}

ui::SettingsCategory EmulatorSettings::page(std::vector<ui::SettingsRow> layerRows) const {
    ui::SettingsCategory page{std::string{categoryId}, "Emulators", {}};
    std::size_t updates = 0;
    for (const EmulatorEntry& entry : found_.catalog().entries()) {
        updates += downloads_.updateAvailable(entry.id) ? 1 : 0;
    }
    page.rows.push_back(
        ui::makeRow(std::string{checkId}, RowKind::Action, "Check for updates",
                    "Reads each emulator's latest release; nothing updates by itself",
                    checkValue(downloads_.check(), updates)));
    std::ranges::move(layerRows, std::back_inserter(page.rows));
    for (const EmulatorEntry& entry : found_.catalog().entries()) {
        page.rows.push_back(ui::makeRow(std::string{emulatorId} + entry.id, RowKind::Action,
                                        entry.name, noteOf(entry), valueOf(entry)));
        const EmulatorInstall* install = found_.installed(entry.id);
        if (install != nullptr && library::roms::downloadedByOpensu(install->source)) {
            page.rows.push_back(ui::makeRow(
                std::string{removeId} + entry.id, RowKind::Action, "Remove " + entry.name,
                entry.builtIn() ? "Deletes the core openSU downloaded; saves stay"
                                : "Deletes the AppImage openSU downloaded; its settings stay",
                armed_ == entry.id ? "Press again to remove" : "Remove"));
        }
    }
    addBiosRows(page);
    return page;
}

namespace {

/// A BIOS file's row value: in use (with the region a scanning core files it under), there but
/// not taken, or missing.
std::string biosValue(const library::roms::CoreBios::File& file) {
    std::string value = file.used ? "In use" : file.present ? "Not used" : "Missing";
    if (!file.region.empty()) {
        value += " · " + file.region;
    }
    return value;
}

} // namespace

void EmulatorSettings::addBiosRows(ui::SettingsCategory& page) const {
    // Each chosen built-in core once, with the systems it runs; arcade BIOS sets are romsets.
    std::vector<std::pair<const EmulatorEntry*, std::string>> chosen;
    for (const std::string& system : found_.catalog().servedSystems()) {
        const EmulatorEntry* entry = found_.catalog().findByName(found_.chosenName(system));
        if (entry == nullptr || !entry->builtIn() || !entry->romsets.empty()) {
            continue;
        }
        const auto at = std::ranges::find(chosen, entry, &decltype(chosen)::value_type::first);
        if (at == chosen.end()) {
            chosen.emplace_back(entry, library::roms::systemLabel(system));
        } else {
            at->second += ", " + library::roms::systemLabel(system);
        }
    }
    const library::roms::EmulatorSearch& search = downloads_.setup().search;
    for (const auto& [entry, systems] : chosen) {
        const std::string row = std::string{biosId} + entry->id;
        const std::optional<std::string> info = search.cores.info(entry->core);
        if (!info) {
            page.rows.push_back(ui::makeRow(
                row, RowKind::Info, systems + " BIOS",
                entry->name + " has no core info; Check for updates fetches it", "Not listed"));
            continue;
        }
        const library::roms::CoreBios bios = library::roms::biosIn(
            library::roms::firmwareIn(*info), entry->withoutBios, entry->biosScan, search.system);
        if (bios.files.empty()) {
            continue;
        }
        page.rows.push_back(ui::makeRow(row, RowKind::Info, systems + " BIOS",
                                        entry->name + " · files in " + search.system.string(),
                                        bios.mode()));
        for (const library::roms::CoreBios::File& file : bios.files) {
            page.rows.push_back(ui::makeRow(
                row + "/" + file.firmware.path, RowKind::Info, file.firmware.path,
                file.firmware.description + (file.firmware.optional ? "" : " · required"),
                biosValue(file)));
        }
    }
}

void EmulatorSettings::run(const std::string& id, int step) {
    const std::string_view row{id};
    if (!row.starts_with(removeId)) {
        armed_.reset();
    }
    if (row == checkId) {
        if (step == 0) {
            downloads_.checkUpdates();
        }
        return;
    }
    if (step != 0) {
        return;
    }
    const bool remove = row.starts_with(removeId);
    const std::string_view key = row.substr(remove ? removeId.size() : emulatorId.size());
    const EmulatorEntry* entry = found_.catalog().find(key);
    if (entry == nullptr) {
        return;
    }
    if (remove) {
        runRemove(*entry);
    } else {
        runEmulator(*entry);
    }
}

void EmulatorSettings::runEmulator(const EmulatorEntry& entry) {
    if (const std::optional<EmulatorDownloads::Progress> progress = downloads_.progress();
        progress && progress->id == entry.id) {
        downloads_.cancel(entry.id);
        return;
    }
    const EmulatorInstall* install = found_.installed(entry.id);
    const bool ours = install != nullptr && library::roms::downloadedByOpensu(install->source);
    if (install != nullptr && install->problem.empty() && !ours) {
        hooks_.say(entry.name + " is installed as " +
                       std::string{library::roms::label(install->source)} + ": " + install->where,
                   false);
        return;
    }
    if (ours && !downloads_.updateAvailable(entry.id)) {
        hooks_.say(downloads_.latest(entry.id)
                       ? entry.name + " is up to date"
                       : "Check for updates to see if " + entry.name + " has a newer release",
                   false);
        return;
    }
    if (const std::string refused = downloads_.start(entry.id); !refused.empty()) {
        hooks_.say(refused, true);
    }
}

void EmulatorSettings::runRemove(const EmulatorEntry& entry) {
    if (armed_ != entry.id) {
        armed_ = entry.id;
        return;
    }
    armed_.reset();
    if (const std::string refused = downloads_.remove(entry.id); !refused.empty()) {
        hooks_.say(entry.name + " was not removed: " + refused, true);
        return;
    }
    hooks_.say(entry.name + " removed", false);
    rescan();
}

} // namespace opensu::app
