#include "emulator_settings.hpp"

#include <algorithm>
#include <vector>

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
constexpr std::string_view systemId = "emulators.system.";

std::string systemLabel(std::string_view key) {
    const library::roms::RomSystem* system = library::roms::systemByKey(key);
    return system != nullptr ? std::string{system->label} : std::string{key};
}

std::string systemsOf(const EmulatorEntry& entry) {
    std::string text;
    for (const std::string& system : entry.systems) {
        text += (text.empty() ? "" : ", ") + systemLabel(system);
    }
    return text;
}

/// The systems the catalog serves, in the order the library lists systems.
std::vector<std::string> servedSystems(const library::roms::EmulatorCatalog& catalog) {
    std::vector<std::string> systems;
    for (const library::roms::RomSystem& system : library::roms::romSystems()) {
        const bool served = std::ranges::any_of(catalog.entries(), [&system](const auto& entry) {
            return entry.serves(system.key);
        });
        if (served) {
            systems.emplace_back(system.key);
        }
    }
    return systems;
}

std::vector<std::string> namesFor(const library::roms::EmulatorCatalog& catalog,
                                  std::string_view system) {
    std::vector<std::string> names{""};
    for (const EmulatorEntry& entry : catalog.entries()) {
        if (entry.serves(system)) {
            names.push_back(entry.name);
        }
    }
    return names;
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
    if (install != nullptr && install->source == InstallSource::Downloaded) {
        const std::optional<library::roms::ReleaseAsset> newest = downloads_.latest(entry.id);
        if (!newest) {
            return "Installed";
        }
        return downloads_.updateAvailable(entry.id) ? "Update to " + newest->version : "Up to date";
    }
    if (install != nullptr && install->problem.empty()) {
        return "Installed";
    }
    return entry.release ? "Download" : "Install it yourself";
}

ui::SettingsCategory EmulatorSettings::page() const {
    ui::SettingsCategory page{std::string{categoryId}, "Emulators", {}};
    std::size_t updates = 0;
    for (const EmulatorEntry& entry : found_.catalog().entries()) {
        updates += downloads_.updateAvailable(entry.id) ? 1 : 0;
    }
    page.rows.push_back(
        ui::makeRow(std::string{checkId}, RowKind::Action, "Check for updates",
                    "Reads each emulator's latest release; nothing updates by itself",
                    checkValue(downloads_.check(), updates)));
    for (const EmulatorEntry& entry : found_.catalog().entries()) {
        page.rows.push_back(ui::makeRow(std::string{emulatorId} + entry.id, RowKind::Action,
                                        entry.name, noteOf(entry), valueOf(entry)));
        const EmulatorInstall* install = found_.installed(entry.id);
        if (install != nullptr && install->source == InstallSource::Downloaded) {
            page.rows.push_back(ui::makeRow(
                std::string{removeId} + entry.id, RowKind::Action, "Remove " + entry.name,
                "Deletes the AppImage openSU downloaded; its settings stay",
                armed_ == entry.id ? "Press again to remove" : "Remove"));
        }
    }
    const settings::Settings& chosen = preferences_.values();
    for (const std::string& system : servedSystems(found_.catalog())) {
        const auto pick = chosen.emulators.systems().find(system);
        const std::string automatic = found_.chosenName(system);
        const std::string value =
            pick != chosen.emulators.systems().end()
                ? pick->second
                : "Automatic" + (automatic.empty() ? std::string{} : " (" + automatic + ")");
        page.rows.push_back(ui::makeRow(std::string{systemId} + system, RowKind::Choice,
                                        systemLabel(system) + " games",
                                        "Runs games that have no emulator of their own", value));
    }
    return page;
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
    if (row.starts_with(systemId)) {
        stepSystem(std::string{row.substr(systemId.size())}, step);
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
    const bool ours = install != nullptr && install->source == InstallSource::Downloaded;
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

void EmulatorSettings::stepSystem(const std::string& system, int step) {
    const std::vector<std::string> names = namesFor(found_.catalog(), system);
    library::EmulatorChoices& choices = preferences_.values().emulators;
    const auto pick = choices.systems().find(system);
    const auto at =
        std::ranges::find(names, pick == choices.systems().end() ? std::string{} : pick->second);
    const auto from = at == names.end() ? 0 : static_cast<int>(at - names.begin());
    const auto count = static_cast<int>(names.size());
    const int next = (from + (step == 0 ? 1 : step) + count) % count;
    choices.setSystem(system, names[static_cast<std::size_t>(next)]);
    preferences_.save();
    hooks_.picksChanged();
}

} // namespace opensu::app
