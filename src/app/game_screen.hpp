// game_screen — how openSU's window relates to a running game. Inside Gamescope the window is the
// overlay over the game and is shown only while a menu or page is up; elsewhere it is hidden while
// the game runs and shown only for those. One owner of the window flag, the overlay and the
// game-time key watcher.
#pragma once

#include <memory>

#include "gamescope_overlay.hpp"
#include "session/game_keys.hpp"

namespace opensu::app {

class GameScreen {
  public:
    /// `hidden` is a maintainer run, whose window is never mapped.
    explicit GameScreen(bool hidden) noexcept : hidden_{hidden} {
    }

    /// Inside Gamescope: the window `handle` becomes the overlay and keys are watched in-game.
    void attachOverlay(unsigned long handle);

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
    bool hidden_;
    bool shown_{false};
    std::unique_ptr<session::GamescopeOverlay> overlay_;
    std::unique_ptr<session::GameKeys> keys_;
};

} // namespace opensu::app
