// game_overlay — openSU's window composited over a running game, where the platform has such an
// overlay, and the shortcuts pressed while the game has the keyboard. One backend per OS, picked in
// CMakeLists.txt: Gamescope's overlay and XInput2 game keys on Linux
// (`game_overlay_gamescope.cpp`); on Windows and macOS the window itself, lowered behind the game
// and raised over it (`game_overlay_raised_window.cpp`); on Android the shell's Activity brought in
// front of the runner's (`game_overlay_activity_task.cpp`). Where none is opened (outside the
// shell's session) `GameScreen` hides and shows the window instead.
#pragma once

#include <memory>
#include <vector>

#include "display/overlay_mode.hpp"
#include "input/shortcuts.hpp"
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
    virtual void show(display::OverlayMode mode) = 0;
    /// The game's windows are gone: the window is an ordinary app again and takes focus back.
    virtual void leave() = 0;
    /// The actions `shortcuts` performs that work with a game up and were pressed since the last
    /// call. Never blocks.
    [[nodiscard]] virtual std::vector<input::Action>
    gameKeys(const input::Shortcuts& shortcuts) = 0;
};

/// The overlay over `window`, or null where the platform has none. Throws std::runtime_error when
/// the platform has one that cannot be opened.
[[nodiscard]] std::unique_ptr<GameOverlay> openGameOverlay(ShellWindow& window);

} // namespace opensu::app
