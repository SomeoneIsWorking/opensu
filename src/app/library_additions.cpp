#include "library_additions.hpp"

#include <string_view>

#include "integration_options.hpp"
#include "integration_settings.hpp"
#include "library/add_game_offer.hpp"

namespace opensu::app {
namespace {

constexpr std::string_view addGameId = "add-game";
constexpr std::string_view addRomFolderId = "add-rom-folder";
constexpr std::string_view lanImportId = "lan-import";
/// An integration's tile is this and its entry's id.
constexpr std::string_view setUpPrefix = "set-up:";

} // namespace

std::vector<library::AddTile> LibraryAdditions::tiles() const {
    std::vector<library::AddTile> tiles;
    if (const std::string_view offer = library::addGameOffer(); !offer.empty()) {
        tiles.push_back({std::string{addGameId}, "Add a game", std::string{offer}});
    }
    tiles.push_back({std::string{addRomFolderId}, "Add a ROM folder", "One folder per console"});
    if (hooks_.importFromLan) {
        tiles.push_back(
            {std::string{lanImportId}, "Import from a PC on your network", "From openSU"});
    }
    const settings::IntegrationChoices chosen = integrations_.choices();
    for (const IntegrationEntry& entry : integrationEntries()) {
        if (!switchOf(chosen, entry.id)) {
            tiles.push_back({std::string{setUpPrefix} + std::string{entry.id},
                             std::string{entry.tile}, "In Settings"});
        }
    }
    return tiles;
}

void LibraryAdditions::press(const std::string& id) const {
    if (id == addGameId) {
        hooks_.addGame();
    } else if (id == addRomFolderId) {
        hooks_.addRomFolder();
    } else if (id == lanImportId && hooks_.importFromLan) {
        hooks_.importFromLan();
    } else if (id.starts_with(setUpPrefix)) {
        hooks_.setUp(IntegrationSettings::rowId(std::string_view{id}.substr(setUpPrefix.size())));
    }
}

} // namespace opensu::app
