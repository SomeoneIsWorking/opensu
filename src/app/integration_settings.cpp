#include "integration_settings.hpp"

#include <algorithm>
#include <vector>

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
constexpr const char* alwaysId = "integrations.always";

std::string rowId(std::string_view entry) {
    return std::string{prefix} + std::string{entry};
}

std::string_view modeNote(SteamMode mode) {
    switch (mode) {
    case SteamMode::Managed:
        return "openSU's own Steam, ended with openSU";
    case SteamMode::Isolated:
        return "openSU's own Steam, kept off the desktop";
    case SteamMode::Normal:
        break;
    }
    return "Your desktop Steam, left running as it is";
}

SteamMode modeOf(SteamUse use) {
    switch (use) {
    case SteamUse::Managed:
        return SteamMode::Managed;
    case SteamUse::Isolated:
        return SteamMode::Isolated;
    case SteamUse::Normal:
    case SteamUse::Off:
        break;
    }
    return SteamMode::Normal;
}

/// The entry `step` places from `now` in `offered`, wrapping; A steps forward.
template <typename T> T stepped(const std::vector<T>& offered, T now, int step) {
    const auto at = std::ranges::find(offered, now);
    const auto from = at == offered.end() ? 0 : at - offered.begin();
    const auto count = static_cast<long>(offered.size());
    const long next = (from + (step == 0 ? 1 : step) % count + count) % count;
    return offered[static_cast<std::size_t>(next)];
}

} // namespace

ui::SettingsCategory IntegrationSettings::page() const {
    const settings::IntegrationChoices choices = integrations_.choices();
    const settings::Integrations& now = integrations_.current();
    ui::SettingsCategory page{std::string{categoryId}, "Integrations", {}};
    page.rows.push_back(ui::makeRow(alwaysId, RowKind::Info, "ROMs and libretro",
                                    "ROM folders, played on libretro", "Always on"));
    for (const IntegrationEntry& entry : integrationEntries()) {
        page.rows.push_back(ui::makeToggle(rowId(entry.id), std::string{entry.label},
                                           std::string{entry.note}, switchOf(choices, entry.id)));
        if (entry.id != "steam" || !choices.steam) {
            continue;
        }
        const SteamMode mode = modeOf(now.steam);
        // A build whose Steam has one mode has nothing to choose.
        if (integrations_.modesOffered().size() > 1) {
            page.rows.push_back(ui::makeRow(modeId, RowKind::Choice, "Steam mode",
                                            std::string{modeNote(mode)},
                                            std::string{settings::label(mode)}));
        }
        if (mode == SteamMode::Isolated) {
            page.rows.push_back(ui::makeRow(padsId, RowKind::Choice, "Steam sees",
                                            std::string{settings::note(now.steamPads)},
                                            std::string{settings::label(now.steamPads)}));
        }
    }
    return page;
}

bool IntegrationSettings::owns(const std::string& id) {
    return id.starts_with(prefix);
}

void IntegrationSettings::run(const std::string& id, int step) {
    settings::IntegrationChoices choices = integrations_.choices();
    const settings::Integrations& now = integrations_.current();
    if (id == modeId) {
        choices.steamMode = stepped(integrations_.modesOffered(), modeOf(now.steam), step);
    } else if (id == padsId) {
        choices.steamPads = stepped(settings::Integrations::padsOffered(), now.steamPads, step);
    } else if (bool* on = switchOf(choices, id.substr(prefix.size())); on != nullptr && step == 0) {
        *on = !*on;
    } else {
        return;
    }
    integrations_.choose(choices);
    changed_();
}

} // namespace opensu::app
