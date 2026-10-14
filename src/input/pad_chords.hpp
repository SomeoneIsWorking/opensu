// pad_chords — pad chords told apart from plain button presses in the shell's controls, by the
// same `gamepad::ChordGate` that keeps chords from a running game. The chords are the ones in force
// (`ChordSet`: the shell's and the running game's); each source (a pad, or the keys and the control
// channel together) has its own gate, so a modifier on one pad starts no chord on another.
#pragma once

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "chord_set.hpp"
#include "gamepad/chord_gate.hpp"
#include "gamepad/event.hpp"
#include "shortcuts.hpp"

namespace opensu::input {

class PadChords {
  public:
    using Clock = gamepad::ChordGate::Clock;

    explicit PadChords(const ChordSet& chords) noexcept : chords_{&chords} {
    }

    /// What `events` come to once chords are taken out.
    struct Result {
        /// The events that are not part of a chord, in order.
        std::vector<gamepad::Event> events;
        /// The actions the chords performed, in order; a game chord that sends pad buttons is
        /// played by the pads and performs none.
        std::vector<Action> actions;
    };

    /// `events`, all heard at `at`.
    [[nodiscard]] Result feed(const std::vector<gamepad::Event>& events, Clock::time_point at);

    /// The modifier held alone for the hold delay as of `at`, until it is released, plays a chord
    /// or is let out.
    [[nodiscard]] std::optional<gamepad::Button> longHeld(Clock::time_point at);

  private:
    struct LongHold {
        std::string source;
        gamepad::Button modifier;
    };

    /// Follows a gate's long-hold steps; false for any other step.
    bool noteLongHold(const std::string& source, const gamepad::ChordGate::Step& step);

    const ChordSet* chords_;
    std::optional<LongHold> longHold_;
    /// One gate per `Event::source`.
    std::map<std::string, gamepad::ChordGate> gates_;
};

} // namespace opensu::input
