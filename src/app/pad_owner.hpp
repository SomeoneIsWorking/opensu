// pad_owner — who has the pads (`gamepad::Claim`), from what openSU is doing and whose window
// Gamescope has focused (`session::FocusHolder`).
//
// Inside Gamescope the shell grabs the pads while its own window is in front, so the Steam client
// it runs never sees them (Guide would open Big Picture). A game openSU launched gets them as
// virtual pads. Any other window in front gets the real pads, and the shell then acts only on
// Guide, which always belongs to openSU. A menu up over a game is openSU's, whatever is focused
// under it. Outside Gamescope there is no focus to follow and nothing else to keep the pads from,
// so the shell leaves them free; a game still holds them.
//
// At launch the game's mapping goes to the chords in force (its game chords) and to the pads
// (`gamepad::ForwardMap`); at its end both are dropped. The chords in force reach the pads whenever
// they change, and the game's virtual pads rest while a menu is up.
#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "gamepad/event.hpp"
#include "gamepad/pads.hpp"
#include "input/button_map.hpp"
#include "input/chord_set.hpp"
#include "session/window_focus.hpp"

namespace opensu::app {

class PadOwner {
  public:
    /// What decides the claim.
    struct Situation {
        /// A game is launching or running.
        bool game{false};
        /// Gamescope's focus is followed, so the shell may grab.
        bool followsFocus{false};
        session::FocusHolder holder{session::FocusHolder::OpenSU};
        /// A menu or page of openSU's is up over the game.
        bool menuOver{false};
    };

    /// Where pads are read from: the system's, or an empty directory in a hidden run, which keeps
    /// no pads.
    [[nodiscard]] static std::filesystem::path inputDirectory(const std::filesystem::path& dataDir,
                                                              bool hidden);
    /// The claim for `situation`.
    [[nodiscard]] static gamepad::Claim claimFor(const Situation& situation);
    /// Whether the shell acts on every pad event, rather than on Guide alone.
    [[nodiscard]] static bool listens(const Situation& situation);

    PadOwner(gamepad::Pads& pads, input::ChordSet& chords) noexcept : pads_{pads}, chords_{chords} {
    }

    /// Holds the pads for a game playing with `map`; returns what the game's environment needs
    /// (`gamepad::Pads::hold`).
    std::vector<std::string> hold(const input::ButtonMap& map);
    /// The game is over: its chords are dropped and the pads go back to the shell's claim.
    void release();
    [[nodiscard]] bool held() const noexcept {
        return situation_.game;
    }
    /// Rests the game's virtual pads while a menu is up; nothing while no game holds them.
    void setBlocked(bool blocked);
    /// Gives the pads the chords in force now.
    void syncChords();

    /// Applies the focus as it is now; the pads change hands only when the claim does.
    void follow(bool followsFocus, session::FocusHolder holder, bool menuOver);
    /// The events the shell acts on: all of them while it listens, else Guide and controllers
    /// coming and going.
    [[nodiscard]] std::vector<gamepad::Event> admit(std::vector<gamepad::Event> events) const;

  private:
    gamepad::Pads& pads_;
    input::ChordSet& chords_;
    Situation situation_;
    gamepad::Claim claim_{gamepad::Claim::Free};
};

} // namespace opensu::app
