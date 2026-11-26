// library_additions — Library's one Add tile, last on its shelf and alone on an empty Home, and the
// "Add to library" page it opens: each integration this build offers that the player has not
// added, then what adds games: Add a game and Add a game folder where this OS can start a game of
// the player's own (`library::addGameOffer`), Add a ROM folder, and Import from a PC on your
// network while LAN is on. A pick closes the page and starts that flow; an integration's starts
// its wizard.
#pragma once

#include <functional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "gpu2d/ui/navigator.hpp"

#include "integrations_in_force.hpp"
#include "library/shelf.hpp"
#include "ui/row_widgets.hpp"
#include "ui/screen_stack.hpp"
#include "ui/setting_row.hpp"

namespace opensu::app {

class LibraryAdditions final : public ui::RowActions {
  public:
    struct Hooks {
        /// Add a game: a file, Steam's shortcuts, Lutris.
        std::function<void()> addGame;
        /// Add a game folder.
        std::function<void()> addGameFolder;
        /// Add a ROM folder.
        std::function<void()> addRomFolder;
        /// Import from a PC on your network.
        std::function<void()> importFromLan;
        /// Adds the integration entry `entry` (`IntegrationWizard::start`).
        std::function<void(std::string_view entry)> addIntegration;
    };

    LibraryAdditions(const IntegrationsInForce& integrations, gpu2d::ui::Navigator& navigator,
                     const ui::Chrome& chrome, Hooks hooks)
        : integrations_{integrations}, navigator_{navigator}, chrome_{chrome},
          hooks_{std::move(hooks)} {
    }

    /// The Add tile, as the shelves take their add tiles.
    [[nodiscard]] std::vector<library::AddTile> tiles() const;
    /// A press on the tile `id`: opens the page.
    void press(const std::string& id);
    /// The page's title and rows as they stand now.
    [[nodiscard]] ui::SettingsCategory page() const;
    /// A pick on the page: closes it and starts what row `id` stands for.
    void run(const std::string& id, int step) override;

  private:
    const IntegrationsInForce& integrations_;
    gpu2d::ui::Navigator& navigator_;
    const ui::Chrome& chrome_;
    Hooks hooks_;
};

} // namespace opensu::app
