// game_keys — keyboard shortcuts the player presses while a game has the keyboard.
//
// Gamescope hands its keys to the focused window on its Xwayland, so iideck's own window
// gets none while a game runs. XInput2 raw key events selected on the root reach every
// client whatever the focus, and only while the desktop gives Gamescope the keyboard.
// The game still receives the keys: nothing is grabbed.
#pragma once

#include <memory>
#include <optional>
#include <vector>

namespace iideck::session {

enum class GameShortcut {
    /// Shift+Tab, Steam's overlay key: the shell treats it as a Guide press.
    Guide,
};

/// Which key went down or up, as far as the shortcuts care.
enum class ShortcutKey {
    LeftShift,
    RightShift,
    Tab,
    Other,
};

/// Turns key edges into shortcuts. A held Tab's repeats fire once.
class ShortcutChord {
  public:
    [[nodiscard]] std::optional<GameShortcut> key(ShortcutKey key, bool pressed);

  private:
    bool leftShift_{};
    bool rightShift_{};
    bool tabDown_{};
};

class GameKeys {
  public:
    /// Opens its own connection to the X display and selects raw key events on the root.
    /// Throws std::runtime_error when the display or XInput 2.2 is missing.
    GameKeys();
    ~GameKeys();

    GameKeys(const GameKeys&) = delete;
    GameKeys& operator=(const GameKeys&) = delete;

    /// The shortcuts pressed since the last poll. Never blocks.
    [[nodiscard]] std::vector<GameShortcut> poll();

  private:
    struct Connection;
    std::unique_ptr<Connection> connection_;
    ShortcutChord chord_;
};

} // namespace iideck::session
