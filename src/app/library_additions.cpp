#include "library_additions.hpp"

#include <algorithm>
#include <memory>
#include <string_view>

#include "integration_options.hpp"
#include "library/add_game_offer.hpp"
#include "row_page_screen.hpp"

namespace opensu::app {
namespace {

constexpr std::string_view addId = "add";
constexpr std::string_view addGameId = "add-game";
constexpr std::string_view addGameFolderId = "add-game-folder";
constexpr std::string_view addRomFolderId = "add-rom-folder";
constexpr std::string_view lanImportId = "lan-import";
/// An integration's row is this and its entry's id.
constexpr std::string_view setUpPrefix = "set-up:";
/// A store's sign-in row is this and its entry's id.
constexpr std::string_view signInPrefix = "sign-in:";

} // namespace

std::vector<library::AddTile> LibraryAdditions::tiles() const {
    return {{std::string{addId}, "Add", "Stores, consoles, games"}};
}

void LibraryAdditions::press(const std::string& id) {
    if (id == addId) {
        navigator_.push(std::make_unique<RowPageScreen>(
            "library-add",
            [this] {
                return page();
            },
            *this, chrome_));
    }
}

ui::SettingsCategory LibraryAdditions::page() const {
    ui::SettingsCategory page{std::string{addId}, "Add to library", {}};
    const settings::IntegrationChoices& chosen = integrations_.choices();
    const std::vector<library::Source> unsigned_ = hooks_.unsignedStores();
    for (const IntegrationEntry& entry : integrationEntries()) {
        const bool needsSignIn = std::ranges::any_of(unsigned_, [&entry](library::Source source) {
            return integrationOf(source) == entry.id;
        });
        if (switchOf(chosen, entry.id) && needsSignIn) {
            page.rows.push_back(ui::makeRow(
                std::string{signInPrefix} + std::string{entry.id}, ui::RowKind::Action,
                "Sign in to " + std::string{entry.label}, "Its games show once you sign in", {}));
        } else if (!switchOf(chosen, entry.id)) {
            page.rows.push_back(ui::makeRow(std::string{setUpPrefix} + std::string{entry.id},
                                            ui::RowKind::Action, std::string{entry.label},
                                            std::string{entry.note}, {}));
        }
    }
    page.rows.push_back(ui::makeHeading("Games"));
    if (const std::string_view offer = library::addGameOffer(); !offer.empty()) {
        page.rows.push_back(ui::makeRow(std::string{addGameId}, ui::RowKind::Action, "Add a game",
                                        std::string{offer}, {}));
        page.rows.push_back(ui::makeRow(std::string{addGameFolderId}, ui::RowKind::Action,
                                        "Add a game folder", "Shortcuts or games", {}));
    }
    page.rows.push_back(ui::makeRow(std::string{addRomFolderId}, ui::RowKind::Action,
                                    "Add a ROM folder", "One folder per console", {}));
    if (chosen.lan) {
        page.rows.push_back(ui::makeRow(std::string{lanImportId}, ui::RowKind::Action,
                                        "Import from a PC on your network",
                                        "Games another openSU offers", {}));
    }
    return page;
}

void LibraryAdditions::run(const std::string& id, int /*step*/) {
    navigator_.pop();
    if (id == addGameId) {
        hooks_.addGame();
    } else if (id == addGameFolderId) {
        hooks_.addGameFolder();
    } else if (id == addRomFolderId) {
        hooks_.addRomFolder();
    } else if (id == lanImportId) {
        hooks_.importFromLan();
    } else if (id.starts_with(setUpPrefix)) {
        hooks_.addIntegration(std::string_view{id}.substr(setUpPrefix.size()));
    } else if (id.starts_with(signInPrefix)) {
        const std::string_view entry = std::string_view{id}.substr(signInPrefix.size());
        for (const library::Source source : hooks_.unsignedStores()) {
            if (integrationOf(source) == entry) {
                hooks_.signIn(source);
            }
        }
    }
}

} // namespace opensu::app
