// details_controller — the details page's buttons and what they do: play or install the game, pick
// its emulator, hide it, go back. Opens for the focused game and keeps what it shows current while
// the library changes under it.
#pragma once

#include <functional>
#include <optional>
#include <string>

#include "audio/sound_player.hpp"
#include "gamepad/event.hpp"
#include "library/game.hpp"
#include "preferences.hpp"
#include "ui/details_page.hpp"

namespace opensu::app {

class DetailsController {
  public:
    /// What the buttons do, in the shell app's terms.
    struct Hooks {
        /// The game the grid has focused, or null when none.
        std::function<const library::Game*()> focused;
        /// The game with this id as the grid shows it now, or nothing when it is gone.
        std::function<std::optional<library::Game>(const std::string& id)> find;
        /// Launches `game`, or offers to install it.
        std::function<void(const library::Game& game)> launch;
        /// Hides or shows `game`, and shows the shelf again.
        std::function<void(const library::Game& game, bool hidden)> setHidden;
        /// Runs ROM `game` on emulator `name` from now on.
        std::function<void(const library::Game& game, const std::string& name)> chooseEmulator;
    };

    DetailsController(ui::DetailsPage& page, audio::SoundPlayer& sounds,
                      const Preferences& preferences, Hooks hooks)
        : page_{page}, sounds_{sounds}, preferences_{preferences}, hooks_{std::move(hooks)} {
    }

    /// Opens the page for the focused game; nothing when no game has focus.
    void open();
    /// Closes the page with iiSU's Close sound.
    void close();
    /// A button while the page is up: it takes them all.
    void act(gamepad::Button button);
    /// Shows the game again as the grid has it now; the page closes when the game is gone.
    void refresh();

  private:
    [[nodiscard]] ui::DetailsView viewOf(const library::Game& game) const;
    void run(ui::DetailsAction action);
    /// Picks the emulator `delta` places along the game's list, wrapping.
    void cycleEmulator(const library::Game& game, int delta);

    ui::DetailsPage& page_;
    audio::SoundPlayer& sounds_;
    const Preferences& preferences_;
    Hooks hooks_;
};

} // namespace opensu::app
