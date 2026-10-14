// game_screen — how openSU's window relates to a running game. Inside Gamescope the window is the
// overlay over the game and is shown only while a menu or page is up; elsewhere it is hidden while
// the game runs and shown only for those. One owner of the window flag, the overlay and the
// game-time key watcher.
#pragma once

#include <memory>

#include "gamescope_overlay.hpp"
#include "session/game_keys.hpp"
#include "shell_window.hpp"

namespace opensu::app {

class GameScreen {
  public:
    /// `hidden` is a maintainer run, whose window is never mapped.
    explicit GameScreen(bool hidden) noexcept : hidden_{hidden} {
    }

    /// The window it shows and hides, once it is open.
    void attach(ShellWindow& window) noexcept {
        window_ = &window;
    }
    /// Inside Gamescope: the attached window becomes the overlay and keys are watched in-game.
    void attachOverlay();

    /// A game started or ended. The window and the overlay follow; nothing is shown.
    void setRunning(bool running);
    /// A menu or page is up (`wanted`) over the running game, or it is not. Applied once per
    /// change; returns whether it changed.
    bool setShown(bool wanted);

    /// The game-time key watcher, or null outside Gamescope.
    [[nodiscard]] session::GameKeys* keys() noexcept {
        return keys_.get();
    }

  private:
    /// Shows or hides the attached window, which a hidden run never maps.
    void showWindow(bool shown);

    bool hidden_;
    ShellWindow* window_{};
    bool shown_{false};
    std::unique_ptr<session::GamescopeOverlay> overlay_;
    std::unique_ptr<session::GameKeys> keys_;
};

} // namespace opensu::app
