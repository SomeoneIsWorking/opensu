// game_overlay — openSU's window composited over a running game, where the platform has such an
// overlay, and the shortcuts pressed while the game has the keyboard. One backend per OS, picked in
// CMakeLists.txt: Gamescope's overlay and XInput2 game keys on Linux
// (`game_overlay_gamescope.cpp`); none elsewhere (`game_overlay_none.cpp`), where `GameScreen`
// lowers and raises the window instead.
#pragma once

#include <memory>
#include <vector>

#include "input/shortcuts.hpp"
#include "session/overlay_mode.hpp"
#include "shell_window.hpp"

namespace opensu::app {

class GameOverlay {
  public:
    GameOverlay() = default;
    GameOverlay(const GameOverlay&) = delete;
    GameOverlay& operator=(const GameOverlay&) = delete;
    GameOverlay(GameOverlay&&) = delete;
    GameOverlay& operator=(GameOverlay&&) = delete;
    virtual ~GameOverlay() = default;

    /// A game is running: the window becomes the overlay, showing nothing.
    virtual void enter() = 0;
    /// Shows or hides the overlay, with the keyboard only when `Interactive`.
    virtual void show(session::OverlayMode mode) = 0;
    /// The game's windows are gone: the window is an ordinary app again and takes focus back.
    virtual void leave() = 0;
    /// The actions `shortcuts` performs that work with a game up and were pressed since the last
    /// call. Never blocks.
    [[nodiscard]] virtual std::vector<input::Action>
    gameKeys(const input::Shortcuts& shortcuts) = 0;
};

/// The overlay over `window`, or null where the platform has none. Throws std::runtime_error when
/// the platform has one that cannot be opened.
[[nodiscard]] std::unique_ptr<GameOverlay> openGameOverlay(const ShellWindow& window);

} // namespace opensu::app
