#include "integration_options.hpp"

#include <array>

#include "library/cli_stores.hpp"
#include "steam/client_modes.hpp"

namespace opensu::app {
namespace {

constexpr std::array entries{
    IntegrationEntry{"steam", "Steam", "Steam games, played through Steam"},
    IntegrationEntry{"epic", "Epic Games", "Epic games via legendary; needs a sign-in"},
    IntegrationEntry{"gog", "GOG", "GOG games via gogdl; needs a sign-in"},
    IntegrationEntry{"emulators", "Standalone emulators",
                     "Finds and downloads PCSX2, Dolphin, more"},
    IntegrationEntry{"plugins", "Lua plugins", "Play time and extras from Lua plugins"},
};

} // namespace

std::vector<IntegrationEntry> integrationEntries() {
    std::vector<IntegrationEntry> offered;
    for (const IntegrationEntry& entry : entries) {
        const bool served = entry.id == "steam" ? !steam::clientModes().empty()
                            : entry.id == "epic" || entry.id == "gog" ? library::cliStoresRun()
                                                                      : true;
        if (served) {
            offered.push_back(entry);
        }
    }
    return offered;
}

bool* switchOf(settings::IntegrationChoices& choices, std::string_view id) noexcept {
    if (id == "steam") {
        return &choices.steam;
    }
    if (id == "epic") {
        return &choices.epic;
    }
    if (id == "gog") {
        return &choices.gog;
    }
    if (id == "emulators") {
        return &choices.emulators;
    }
    if (id == "plugins") {
        return &choices.plugins;
    }
    return nullptr;
}

bool switchOf(const settings::IntegrationChoices& choices, std::string_view id) noexcept {
    settings::IntegrationChoices copy = choices;
    const bool* on = switchOf(copy, id);
    return on != nullptr && *on;
}

} // namespace opensu::app
