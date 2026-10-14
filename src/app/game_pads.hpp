// game_pads — the pads as a running game holds them. At launch the game's mapping goes to the
// chords in force (its game chords) and to the pads (`gamepad::ForwardMap`); at its end both are
// dropped. The chords in force reach the pads whenever they change, and the pads rest while a menu
// is up.
#pragma once

#include <string>
#include <vector>

#include "gamepad/pads.hpp"
#include "input/button_map.hpp"
#include "input/chord_set.hpp"

namespace opensu::app {

class GamePads {
  public:
    GamePads(gamepad::Pads& pads, input::ChordSet& chords) noexcept : pads_{pads}, chords_{chords} {
    }

    /// Holds the pads for a game playing with `map`; returns what the game's environment needs
    /// (`gamepad::Pads::hold`).
    std::vector<std::string> hold(const input::ButtonMap& map);
    /// Gives the pads back and drops the game's chords.
    void release();
    [[nodiscard]] bool held() const noexcept {
        return held_;
    }
    /// Rests the game's virtual pads while a menu is up; nothing while no game holds them.
    void setBlocked(bool blocked);
    /// Gives the pads the chords in force now.
    void syncChords();

  private:
    gamepad::Pads& pads_;
    input::ChordSet& chords_;
    bool held_{false};
};

} // namespace opensu::app
