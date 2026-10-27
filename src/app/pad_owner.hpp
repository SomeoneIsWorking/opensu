// pad_owner — who has the pads (`gamepad::Claim`), from what openSU is doing and whose window
// Gamescope has focused (`session::FocusHolder`).
//
// Inside Gamescope the shell grabs the pads while its own window is in front, so the Steam client
// it runs (Managed, Isolated) never sees them (Guide would open Big Picture). With no Steam of
// openSU's to guard against (Steam off, or the desktop's Steam in Normal mode) the shell leaves
// them free. A game openSU launched gets them as virtual pads. Any other window in front gets the
// real pads, and the shell then acts only on Guide, which always belongs to openSU. A menu up over
// a game is openSU's, whatever is focused under it. Outside Gamescope there is no focus to follow
// and nothing else to keep the pads from, so the shell leaves them free; a game still holds them.
//
// At launch the game's mapping goes to the chords in force (its game chords) and to the pads
// (`gamepad::ForwardMap`); at its end both are dropped. The chords in force reach the pads whenever
// they change, and the game's virtual pads rest while a menu is up.
//
// While a page of openSU's own Steam waits on the player in Big Picture (the SteamGridDB sign-in),
// the pads are lent to Steam under the same rule as a game, with Big Picture as the window they
// serve: held as virtual pads with no mapping (Isolated's sandbox sees only those), Game while Big
// Picture is in front, the shell hearing only Guide there, and the virtual pads resting while the
// shell is in front again. Lending ends with the sign-in; a game's hold is never replaced.
#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "gamepad/event.hpp"
#include "gamepad/pad_source.hpp"
#include "input/button_map.hpp"
#include "input/chord_set.hpp"
#include "session/window_focus.hpp"

namespace opensu::app {

class PadOwner {
  public:
    /// What decides the claim.
    struct Situation {
        /// A game is launching or running, or the pads are lent to Steam.
        bool game{false};
        /// The window the held pads serve: the game's, or Big Picture's while lent to Steam.
        session::FocusHolder serves{session::FocusHolder::Game};
        /// Gamescope's focus is followed, so the shell may grab.
        bool followsFocus{false};
        session::FocusHolder holder{session::FocusHolder::OpenSU};
        /// A menu or page of openSU's is up over the game.
        bool menuOver{false};
        /// openSU's own Steam runs in its Gamescope, which the shell keeps the pads from.
        bool guardsSteam{true};
        /// The pads can be grabbed and forwarded (`gamepad::PadSource::grabs`); without it the
        /// claim is only ever Free or Away.
        bool grabs{true};
    };

    /// Which pads a run reads: the system's, or in a hidden run only what is linked into
    /// `<dataDir>/hidden-pads`, which nothing is unless a maintainer tool puts a test pad there.
    [[nodiscard]] static gamepad::PadSourceOptions padsFor(const std::filesystem::path& dataDir,
                                                           bool hidden);
    /// The claim for `situation`.
    [[nodiscard]] static gamepad::Claim claimFor(const Situation& situation);
    /// Whether the shell acts on every pad event, rather than on Guide alone.
    [[nodiscard]] static bool listens(const Situation& situation);

    PadOwner(gamepad::PadSource& pads, input::ChordSet& chords) noexcept
        : pads_{pads}, chords_{chords} {
        situation_.grabs = pads.grabs();
    }

    /// Holds the pads for a game playing with `map`; returns what the game's environment needs
    /// (`gamepad::PadSource::hold`).
    std::vector<std::string> hold(const input::ButtonMap& map);
    /// The game is over: its chords are dropped and the pads go back to the shell's claim.
    void release();
    /// Whether a game holds the pads.
    [[nodiscard]] bool held() const noexcept {
        return situation_.game && situation_.serves == session::FocusHolder::Game;
    }
    /// Lends the pads to openSU's own Steam while `lend`, as to a game shown by Big Picture, and
    /// takes them back after; nothing while a game holds them or Steam is not openSU's.
    void lendToSteam(bool lend);
    /// Rests the game's virtual pads while a menu is up; nothing while no game holds them.
    void setBlocked(bool blocked);
    /// Gives the pads the chords in force now.
    void syncChords();

    /// Applies the focus as it is now; the pads change hands only when the claim does.
    void follow(bool followsFocus, session::FocusHolder holder, bool menuOver);
    /// Whether the shell keeps the pads from openSU's own Steam; applied with the next `follow`.
    void guardSteam(bool guards) noexcept {
        situation_.guardsSteam = guards;
    }
    /// The events the shell acts on: all of them while it listens, else Guide and controllers
    /// coming and going.
    [[nodiscard]] std::vector<gamepad::Event> admit(std::vector<gamepad::Event> events) const;

  private:
    /// Holds the pads with `map` for the window `serves`.
    std::vector<std::string> take(const input::ButtonMap& map, session::FocusHolder serves);
    /// Lent to Steam: the virtual pads rest unless Big Picture is in front.
    void restOutsideBigPicture();

    gamepad::PadSource& pads_;
    input::ChordSet& chords_;
    Situation situation_;
    gamepad::Claim claim_{gamepad::Claim::Free};
    /// Whether the lent virtual pads rest.
    bool resting_{false};
};

} // namespace opensu::app
