#include "integration_settings.hpp"

#include <algorithm>
#include <vector>

#include "gpu2d/ui/screen.hpp"

#include "integration_options.hpp"

namespace opensu::app {
namespace {

using settings::SteamMode;
using settings::SteamPads;
using settings::SteamUse;
using ui::RowKind;

constexpr std::string_view prefix = "integrations.";
constexpr const char* modeId = "integrations.steamMode";
constexpr const char* padsId = "integrations.steamPads";
/// A Remove row is this and its entry's id.
constexpr std::string_view removePrefix = "integrations.remove.";

/// The entry `entry`'s name, as Library's Add page shows it.
std::string labelOf(std::string_view entry) {
    for (const IntegrationEntry& offered : integrationEntries()) {
        if (offered.id == entry) {
            return std::string{offered.label};
        }
    }
    return std::string{entry};
}

/// What removing `entry` does, for the question Remove asks.
std::string removalOf(std::string_view entry) {
    if (entry == "emulators") {
        return "openSU stops finding and downloading standalone emulators; ROMs keep playing on "
               "libretro.";
    }
    if (entry == "plugins") {
        return "Lua plugins stop running, and what they add goes.";
    }
    if (entry == "lan") {
        return "openSU stops finding other openSUs here; their games leave the library.";
    }
    return "Its games leave the library.";
}

SteamMode modeOf(SteamUse use) {
    switch (use) {
    case SteamUse::Managed:
        return SteamMode::Managed;
    case SteamUse::Isolated:
        return SteamMode::Isolated;
    case SteamUse::Normal:
    case SteamUse::Fumarole:
    case SteamUse::Off:
        break;
    }
    return SteamMode::Normal;
}

/// `offered` as a choice's options, `now` chosen.
template <typename T> ui::Options optionsOf(const std::vector<T>& offered, T now) {
    ui::Options options;
    for (const T& entry : offered) {
        options.labels.emplace_back(settings::label(entry));
    }
    const auto at = std::ranges::find(offered, now);
    options.chosen = at == offered.end() ? 0 : static_cast<std::size_t>(at - offered.begin());
    return options;
}

} // namespace

std::vector<ui::SettingsRow> IntegrationSettings::steamRows() const {
    std::vector<ui::SettingsRow> rows;
    const settings::Integrations& now = integrations_.current();
    if (now.steam == SteamUse::Off || now.steam == SteamUse::Fumarole) {
        return rows;
    }
    const SteamMode mode = modeOf(now.steam);
    // A build whose Steam has one mode has nothing to choose.
    if (integrations_.modesOffered().size() > 1) {
        rows.push_back(ui::makeChoice(modeId, "Steam mode", std::string{steamModeNote(mode)},
                                      optionsOf(integrations_.modesOffered(), mode)));
    }
    if (mode == SteamMode::Isolated) {
        rows.push_back(
            ui::makeChoice(padsId, "Steam sees", std::string{settings::note(now.steamPads)},
                           optionsOf(settings::Integrations::padsOffered(), now.steamPads)));
    }
    return rows;
}

ui::SettingsRow IntegrationSettings::removeRow(std::string_view entry) {
    return ui::makeRow(std::string{removePrefix} + std::string{entry}, RowKind::Action,
                       "Remove " + labelOf(entry), "Add it again from Library's Add tile",
                       "Remove");
}

bool IntegrationSettings::owns(const std::string& id) {
    return id.starts_with(prefix);
}

void IntegrationSettings::keep(const settings::IntegrationChoices& choices) {
    if (integrations_.choose(choices)) {
        changed_();
    }
}

void IntegrationSettings::run(const std::string& id, int step) {
    settings::IntegrationChoices choices = integrations_.choices();
    const settings::Integrations& now = integrations_.current();
    if (id == modeId) {
        choices.steamMode = ui::stepped(integrations_.modesOffered(), modeOf(now.steam), step);
        keep(choices);
    } else if (id == padsId) {
        choices.steamPads = ui::stepped(settings::Integrations::padsOffered(), now.steamPads, step);
        keep(choices);
    } else if (id.starts_with(removePrefix) && step == 0) {
        const std::string entry = id.substr(removePrefix.size());
        if (switchOf(choices, entry) == nullptr) {
            return;
        }
        navigator_.push(gpu2d::ui::confirm(
            "Remove " + labelOf(entry) + "?",
            removalOf(entry) + " Add it again from Library's Add tile.", "Remove", [this, entry] {
                settings::IntegrationChoices off = integrations_.choices();
                *switchOf(off, entry) = false;
                keep(off);
            }));
    }
}

} // namespace opensu::app
