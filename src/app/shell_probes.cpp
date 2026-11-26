// shell_probes — the control channel's probes, run on the main loop: each does by name what the
// player's buttons do, through the same owners (details' Play, Guide › Close game, the search,
// Settings, Preferences), so a run can drive and check the shell without walking its screens.
#include <algorithm>
#include <cctype>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <string_view>

#include "library/linked_play.hpp"
#include "shell_app.hpp"

namespace opensu::app {
namespace {

using json = nlohmann::json;

ControlAnswer done(const json& body) {
    return {.status = 200, .body = body.dump()};
}

ControlAnswer refused(int status, std::string reason) {
    return {.status = status, .body = std::move(reason)};
}

bool sameWords(std::string_view a, std::string_view b) {
    return std::ranges::equal(a, b, [](char x, char y) {
        return std::tolower(static_cast<unsigned char>(x)) ==
               std::tolower(static_cast<unsigned char>(y));
    });
}

} // namespace

std::optional<std::size_t> ShellApp::tileOfGame(const std::string& id) const {
    const library::Game* own = ownGame(id);
    const std::vector<library::Game> copies =
        own == nullptr ? std::vector<library::Game>{} : library::copiesOf(games_, *own);
    const std::vector<ui::Tile>& tiles = shell_.grid().tiles();
    for (std::size_t index = 0; index < tiles.size(); ++index) {
        const auto* game = std::get_if<library::Game>(&tiles[index].item);
        if (game != nullptr &&
            (game->id == id ||
             std::ranges::find(copies, game->id, &library::Game::id) != copies.end())) {
            return index;
        }
    }
    return std::nullopt;
}

ControlAnswer ShellApp::runProbe(const ControlCommand& command) {
    using Kind = ControlCommand::Kind;
    const std::string& argument = command.argument;
    switch (command.kind) {
    case Kind::LaunchGame: {
        if (launches_.running()) {
            return refused(409, "a game is running: " + launches_.game().id);
        }
        std::optional<library::Game> game = shownGame(argument);
        if (!game) {
            if (const library::Game* own = ownGame(argument)) {
                game = *own;
            }
        }
        if (!game) {
            return refused(404, "no game " + argument);
        }
        // Details' Play button.
        if (!launch(*game, LaunchRequest{.profile = profiles_.book().active().id})) {
            return refused(409, "the launch was refused; /state's toast says why");
        }
        return done({{"launching", game->id}});
    }
    case Kind::CloseGame: {
        // Guide › Close game, past its question.
        if (remotePlay_.streaming()) {
            remotePlay_.end();
            return done({{"closed", "remote play"}});
        }
        if (!launches_.running()) {
            return refused(409, "no game is running");
        }
        const std::string id = launches_.game().id;
        closeRunningGame();
        return done({{"closed", id}});
    }
    case Kind::OpenDetails:
    case Kind::OpenGameSettings: {
        const library::Game* own = ownGame(argument);
        if (own == nullptr) {
            return refused(404, "no game " + argument);
        }
        closePages();
        std::optional<std::size_t> tile = tileOfGame(argument);
        if (!tile) {
            // Off this shelf: searched for by its title, as the player would find it.
            search_.edit(own->title);
            tile = tileOfGame(argument);
        }
        if (!tile) {
            return refused(404, argument + " has no tile, even searched for by its title");
        }
        shell_.grid().focusTile(*tile);
        details_.open();
        if (command.kind == Kind::OpenGameSettings) {
            // Details' Game settings row.
            details_.openPage(ui::copyRowId(argument, ui::detailsSettingsId));
            return done({{"gameSettings", argument}});
        }
        return done({{"details", argument}});
    }
    case Kind::Search: {
        closePages();
        search_.open();
        search_.edit(argument);
        return done({{"search", search_.text()}, {"results", search_.results().size()}});
    }
    case Kind::OpenSettings: {
        const std::string_view whole{argument};
        const std::size_t slash = whole.find('/');
        const std::string_view wantedCategory = whole.substr(0, slash);
        const std::string_view wantedRow =
            slash == std::string_view::npos ? std::string_view{} : whole.substr(slash + 1);
        for (const ui::SettingsCategory& category : settingsScreen_.categories()) {
            if (category.id != wantedCategory && !sameWords(category.label, wantedCategory)) {
                continue;
            }
            const std::string categoryId = category.id;
            if (wantedRow.empty()) {
                closePages();
                settingsScreen_.openCategory(categoryId);
                return done({{"settings", categoryId}});
            }
            for (const ui::SettingsRow& row : category.rows) {
                if (row.id == wantedRow || sameWords(row.label, wantedRow)) {
                    const std::string rowId = row.id;
                    closePages();
                    settingsScreen_.openAt(categoryId, rowId);
                    return done({{"settings", categoryId}, {"row", rowId}});
                }
            }
            std::string rows;
            for (const ui::SettingsRow& row : category.rows) {
                rows += (rows.empty() ? "" : ", ") + row.label;
            }
            return refused(404, "Settings › " + category.label + " has no row " +
                                    std::string{wantedRow} + "; its rows: " + rows);
        }
        std::string categories;
        for (const ui::SettingsCategory& category : settingsScreen_.categories()) {
            categories += (categories.empty() ? "" : ", ") + category.label;
        }
        return refused(404, "Settings has no category " + std::string{wantedCategory} +
                                "; its categories: " + categories);
    }
    case Kind::OpenDevices: {
        const std::string_view whole{argument};
        const std::size_t slash = whole.find('/');
        const std::string_view wantedTab = whole.substr(0, slash);
        const std::string_view wantedRow =
            slash == std::string_view::npos ? std::string_view{} : whole.substr(slash + 1);
        std::string tabs;
        for (const ui::SettingsCategory& tab : devicesScreen_.tabs()) {
            tabs += (tabs.empty() ? "" : ", ") + tab.label;
            if (tab.id != wantedTab && !sameWords(tab.label, wantedTab)) {
                continue;
            }
            const std::string tabId = tab.id;
            std::string rowId;
            std::string rows;
            for (const ui::SettingsRow& row : tab.rows) {
                rows += (rows.empty() ? "" : ", ") + row.label;
                if (!wantedRow.empty() && rowId.empty() &&
                    (row.id == wantedRow || sameWords(row.label, wantedRow))) {
                    rowId = row.id;
                }
            }
            if (!wantedRow.empty() && rowId.empty()) {
                return refused(404, "Devices › " + tab.label + " has no row " +
                                        std::string{wantedRow} + "; its rows: " + rows);
            }
            closePages();
            devicesScreen_.openAt(tabId, rowId);
            return done({{"devices", tabId}, {"row", rowId}});
        }
        return refused(404, "Devices has no tab " + std::string{wantedTab} + "; its tabs: " + tabs);
    }
    case Kind::GetSettings:
        return {.status = 200, .body = settings::toText(preferences_.values())};
    case Kind::SetSettings: {
        settings::Settings& values = preferences_.values();
        const settings::Settings before = values;
        std::string error;
        if (!settings::patch(values, argument, error)) {
            return refused(400, error);
        }
        // As the Settings screen applies a change.
        preferences_.save();
        applyLayout();
        integrations_.choose(values.integrations);
        applyIntegrations();
        stickCorrectionSettings_.applySaved();
        ringLights_.applySaved();
        if (values.romFolders != before.romFolders || values.gameFolders != before.gameFolders ||
            values.steamRoots != before.steamRoots) {
            libraryFoldersChanged();
        }
        settingsScreen_.refresh();
        return {.status = 200, .body = settings::toText(values)};
    }
    case Kind::LibraryAdd: {
        // Library's Add tile, then the row.
        std::string rows;
        for (const ui::SettingsRow& row : libraryAdditions_.page().rows) {
            if (row.id != argument) {
                rows += (rows.empty() ? "" : ", ") + row.id;
                continue;
            }
            closePages();
            libraryAdditions_.press("add");
            libraryAdditions_.run(argument, 0);
            const gpu2d::ui::Screen* top = shell_.screens().navigator().top();
            return done({{"added", argument}, {"top", top == nullptr ? "" : top->id()}});
        }
        return refused(404, "Library's Add page has no row " + argument + "; its rows: " + rows);
    }
    case Kind::WizardPick: {
        const gpu2d::ui::Screen* top = shell_.screens().navigator().top();
        if (top == nullptr || !top->id().starts_with("wizard/")) {
            return refused(409, "no step of the integration wizard is on top");
        }
        integrationWizard_.run(argument, 0);
        const gpu2d::ui::Screen* now = shell_.screens().navigator().top();
        return done({{"picked", argument}, {"top", now == nullptr ? "" : now->id()}});
    }
    }
    return refused(400, "unknown probe");
}

} // namespace opensu::app
