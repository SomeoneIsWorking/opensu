// shortcut_router — sends what the keyboard, the pad chords and the game-time key watcher say to
// the shell. Button actions come out as the pad events they stand for, so there is one set of
// handlers; every other action (the volume, the Guide menu on Volume, quit, and the game actions a
// chord performs over a running game) is done here. The table they all read is `Shortcuts`.
#pragma once

#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "game_overlay.hpp"
#include "gamepad/event.hpp"
#include "input/keyboard_bindings.hpp"
#include "input/pad_chords.hpp"
#include "input/shortcuts.hpp"
#include "retro_session.hpp"
#include "volume_control.hpp"

namespace opensu::app {

class ShortcutRouter {
  public:
    struct Hooks {
        /// Closes openSU.
        std::function<void()> quit;
        /// Opens the Guide menu on Volume, or closes it.
        std::function<void()> quickMenu;
        /// Whether a game is running, which the game actions need.
        std::function<bool()> gameRunning;
        std::function<void()> closeGame;
        std::function<void()> openDevices;
        std::function<void()> goHome;
        std::function<void()> openDownloads;
        /// Tells the player something; `error` marks a refusal.
        std::function<void(const std::string& text, bool error)> say;
    };

    /// `chords` are the chords in force: the table's and the running game's.
    ShortcutRouter(input::Shortcuts& shortcuts, const input::ChordSet& chords,
                   VolumeControl& volume, RetroSession& retro, Hooks hooks)
        : shortcuts_{shortcuts}, volume_{volume}, retro_{retro}, chords_{chords},
          hooks_{std::move(hooks)} {
    }

    /// What the keys did this frame.
    struct Keys {
        /// The pad events the keys stand for.
        std::vector<gamepad::Event> events;
        /// Whether a shortcut key went down or up, whatever it does.
        bool any{false};
    };

    [[nodiscard]] Keys fromKeys(const input::KeySource& keys);
    /// `events`, heard at `at`, less the chords, which are done here.
    [[nodiscard]] std::vector<gamepad::Event>
    fromPad(const std::vector<gamepad::Event>& events,
            input::PadChords::Clock::time_point at = input::PadChords::Clock::now());
    /// The pad modifier held alone for the hold delay as of `at`, whose chords the shell lists.
    [[nodiscard]] std::optional<gamepad::Button> longHeld(input::PadChords::Clock::time_point at) {
        return chords_.longHeld(at);
    }
    /// Does what the game-time watcher heard, when a game has the keyboard; the watcher is drained
    /// either way, so the shell's own keys are not acted on twice. Over a libretro game
    /// (`backOpensMenu`) Back opens the Guide menu; over any other game it does nothing.
    [[nodiscard]] std::vector<gamepad::Event> fromGame(GameOverlay& overlay, bool gameHasKeyboard,
                                                       bool backOpensMenu);

  private:
    /// The events of `action`'s edge if it is a button, else does it on a press.
    void route(input::Action action, bool pressed, std::vector<gamepad::Event>& out);
    /// Does an action that is not a button.
    void perform(input::Action action);
    /// A game action: refused with a toast when no game runs.
    void onGame(const std::function<void()>& act);

    input::Shortcuts& shortcuts_;
    VolumeControl& volume_;
    RetroSession& retro_;
    input::PadChords chords_;
    Hooks hooks_;
};

} // namespace opensu::app
