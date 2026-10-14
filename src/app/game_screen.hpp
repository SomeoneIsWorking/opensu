// game_screen — how openSU's window relates to a running game. Inside Gamescope the window is the
// overlay over the game: interactive while a menu or page is up, which takes the keyboard and rests
// the game's pads, and passive while the chord list is up, which leaves both to the game (the
// shell still hears every pad edge, so chords play). Elsewhere the window is hidden while the game
// runs and shown only for a menu or page; mapping it would take the focus, so it stays hidden for
// a passive overlay. One owner of the window flag, the overlay, the pads' block and the game-time
// key watcher.
#pragma once

#include <functional>
#include <memory>
#include <utility>

#include "gamescope_overlay.hpp"
#include "session/game_keys.hpp"
#include "shell_window.hpp"

namespace opensu::app {

class GameScreen {
  public:
    /// `hidden` is a maintainer run, whose window is never mapped. `blockPads` rests the game's
    /// pads (true) or gives them back (false).
    GameScreen(bool hidden, std::function<void(bool blocked)> blockPads)
        : hidden_{hidden}, blockPads_{std::move(blockPads)} {
    }

    /// The window it shows and hides, once it is open.
    void attach(ShellWindow& window) noexcept {
        window_ = &window;
    }
    /// Inside Gamescope: the attached window becomes the overlay and keys are watched in-game.
    void attachOverlay();

    /// A game started or ended. The window and the overlay follow; nothing is shown.
    void setRunning(bool running);
    /// What is wanted over the running game: `Interactive` for a menu or page, `Passive` for the
    /// chord list. Applied once per change.
    void show(session::OverlayMode wanted);
    [[nodiscard]] session::OverlayMode mode() const noexcept {
        return mode_;
    }
    /// Inside Gamescope with no game running: puts the window back in front of whatever another
    /// program mapped since. Gamescope focuses the most recently mapped window (steamcompmgr.cpp
    /// is_focus_priority_greater), so the window is mapped again.
    void takeFocus();

    /// The game-time key watcher, or null outside Gamescope.
    [[nodiscard]] session::GameKeys* keys() noexcept {
        return keys_.get();
    }

  private:
    /// Shows or hides the attached window, which a hidden run never maps.
    void showWindow(bool shown);

    bool hidden_;
    bool running_{false};
    std::function<void(bool blocked)> blockPads_;
    ShellWindow* window_{};
    session::OverlayMode mode_{session::OverlayMode::Hidden};
    std::unique_ptr<session::GamescopeOverlay> overlay_;
    std::unique_ptr<session::GameKeys> keys_;
};

} // namespace opensu::app
