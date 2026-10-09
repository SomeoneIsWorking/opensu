// shortcut_router — sends what the keyboard, the pad chords and the game-time key watcher say to
// the shell. Button actions come out as the pad events they stand for, so there is one set of
// handlers; the volume, quick menu and quit actions are done here. The table they all read is `Shortcuts`.
#pragma once

#include <functional>
#include <vector>

#include "gamepad/event.hpp"
#include "input/keyboard_bindings.hpp"
#include "input/pad_chords.hpp"
#include "input/shortcuts.hpp"
#include "session/game_keys.hpp"
#include "volume_control.hpp"

namespace opensu::app {

class ShortcutRouter {
  public:
    struct Hooks {
        /// Closes openSU.
        std::function<void()> quit;
        /// Opens or closes the quick menu.
        std::function<void()> quickMenu;
    };

    ShortcutRouter(input::Shortcuts& shortcuts, VolumeControl& volume, Hooks hooks)
        : shortcuts_{shortcuts}, volume_{volume}, chords_{shortcuts}, hooks_{std::move(hooks)} {
    }

    /// What the keys did this frame.
    struct Keys {
        /// The pad events the keys stand for.
        std::vector<gamepad::Event> events;
        /// Whether a shortcut key went down or up, whatever it does.
        bool any{false};
    };

    [[nodiscard]] Keys fromKeys(const input::KeySource& keys);
    /// A key combination pressed and released, as a tap.
    [[nodiscard]] std::vector<gamepad::Event> fromCombo(const input::Combo& combo);
    /// `events` less the chords, which are done here.
    [[nodiscard]] std::vector<gamepad::Event> fromPad(const std::vector<gamepad::Event>& events);
    /// Does what the game-time watcher heard.
    [[nodiscard]] std::vector<gamepad::Event> fromGame(session::GameKeys& keys);

  private:
    /// The events of `action`'s edge if it is a button, else does it on a press.
    void route(input::Action action, bool pressed, std::vector<gamepad::Event>& out);

    input::Shortcuts& shortcuts_;
    VolumeControl& volume_;
    input::PadChords chords_;
    Hooks hooks_;
};

} // namespace opensu::app
