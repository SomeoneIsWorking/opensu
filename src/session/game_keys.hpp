// game_keys — keyboard shortcuts the player presses while a game has the keyboard (the Guide menu
// and the volume), by the same table the shell reads.
//
// Gamescope hands its keys to the focused window on its Xwayland, so opensu's own window
// gets none while a game runs. XInput2 raw key events selected on the root reach every
// client whatever the focus, and only while the desktop gives Gamescope the keyboard.
// The game still receives the keys: nothing is grabbed.
#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <set>
#include <vector>

#include "input/shortcuts.hpp"

namespace opensu::session {

/// Turns key edges into key combinations. A held key's repeats fire once, and a modifier alone
/// fires nothing.
class ShortcutChord {
  public:
    /// `key` is a raylib key code; the combination it completes when it goes down.
    [[nodiscard]] std::optional<input::Combo> key(int key, bool pressed);

  private:
    bool ctrl_{};
    bool shift_{};
    std::set<int> down_;
};

class GameKeys {
  public:
    /// Opens its own connection to the X display and selects raw key events on the root.
    /// Throws std::runtime_error when the display or XInput 2.2 is missing.
    GameKeys();
    ~GameKeys();

    GameKeys(const GameKeys&) = delete;
    GameKeys& operator=(const GameKeys&) = delete;

    /// The actions `shortcuts` performs that work with a game up and were pressed since the last
    /// poll. Never blocks.
    [[nodiscard]] std::vector<input::Action> poll(const input::Shortcuts& shortcuts);

  private:
    struct Connection;
    std::unique_ptr<Connection> connection_;
    ShortcutChord chord_;
};

} // namespace opensu::session
