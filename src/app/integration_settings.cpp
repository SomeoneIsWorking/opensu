#include "integration_settings.hpp"

#include <algorithm>
#include <vector>

#include "integration_options.hpp"

namespace opensu::app {
namespace {

using settings::SteamMode;
using ui::RowKind;

constexpr std::string_view prefix = "integrations.";
constexpr const char* modeId = "integrations.steamMode";
constexpr const char* isolatedId = "integrations.isolated";
constexpr const char* alwaysId = "integrations.always";

std::string rowId(std::string_view entry) {
    return std::string{prefix} + std::string{entry};
}

std::string_view modeNote(SteamMode mode) {
    return mode == SteamMode::Managed ? "openSU's own Steam, ended with openSU"
                                      : "Your desktop Steam, left running as it is";
}

} // namespace

ui::SettingsCategory IntegrationSettings::page() const {
    const settings::IntegrationChoices choices = integrations_.choices();
    const bool session = integrations_.loginSession();
    ui::SettingsCategory page{std::string{categoryId}, "Integrations", {}};
    page.rows.push_back(ui::makeRow(alwaysId, RowKind::Info, "ROMs and libretro",
                                    "ROM folders, played on libretro", "Always on"));
    for (const IntegrationEntry& entry : integrationEntries()) {
        page.rows.push_back(ui::makeToggle(rowId(entry.id), std::string{entry.label},
                                           std::string{entry.note}, switchOf(choices, entry.id)));
        if (entry.id != "steam" || !choices.steam) {
            continue;
        }
        const std::vector<SteamMode> offered = settings::Integrations::modesOffered(session);
        const SteamMode mode = integrations_.current().steam == settings::SteamUse::Managed
                                   ? SteamMode::Managed
                                   : SteamMode::Normal;
        if (offered.size() > 1) {
            page.rows.push_back(ui::makeRow(modeId, RowKind::Choice, "Steam mode",
                                            std::string{modeNote(mode)},
                                            std::string{settings::label(mode)}));
        } else {
            page.rows.push_back(ui::makeRow(modeId, RowKind::Info, "Steam mode",
                                            "A login session has no desktop Steam",
                                            std::string{settings::label(mode)}));
        }
        page.rows.push_back(ui::makeRow(isolatedId, RowKind::Info, "Isolated Steam",
                                        "Needs a sandbox check before it can be built",
                                        "Not available yet"));
    }
    return page;
}

bool IntegrationSettings::owns(const std::string& id) {
    return id.starts_with(prefix);
}

void IntegrationSettings::run(const std::string& id, int step) {
    settings::IntegrationChoices choices = integrations_.choices();
    if (id == modeId) {
        const std::vector<SteamMode> offered =
            settings::Integrations::modesOffered(integrations_.loginSession());
        const SteamMode now = integrations_.current().steam == settings::SteamUse::Managed
                                  ? SteamMode::Managed
                                  : SteamMode::Normal;
        const auto at = std::ranges::find(offered, now);
        const auto from = at == offered.end() ? 0 : at - offered.begin();
        const auto count = static_cast<long>(offered.size());
        const long next = (from + (step == 0 ? 1 : step) + count) % count;
        choices.steamMode = offered[static_cast<std::size_t>(next)];
    } else if (bool* on = switchOf(choices, id.substr(prefix.size())); on != nullptr && step == 0) {
        *on = !*on;
    } else {
        return;
    }
    integrations_.choose(choices);
    changed_();
}

} // namespace opensu::app
