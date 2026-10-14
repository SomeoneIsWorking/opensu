// pad_chords — pad chords told apart from plain button presses in the shell's controls, by the
// same `gamepad::ChordGate` that keeps chords from a running game. The chords are the ones the
// `Shortcuts` table binds; each source (a pad, or the keys and the control channel together) has
// its own gate, so a modifier on one pad starts no chord on another.
#pragma once

#include <chrono>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "gamepad/chord_gate.hpp"
#include "gamepad/event.hpp"
#include "shortcuts.hpp"

namespace opensu::input {

/// How long a modifier held alone may be and still be a tap when released. Longer, its release
/// does nothing.
inline constexpr std::chrono::milliseconds holdDelay{500};

/// Every pad chord `shortcuts` binds, with the hold delay: what a gate needs.
[[nodiscard]] gamepad::ChordTriggers chordTriggers(const Shortcuts& shortcuts);

class PadChords {
  public:
    using Clock = gamepad::ChordGate::Clock;

    explicit PadChords(const Shortcuts& shortcuts) noexcept : shortcuts_{&shortcuts} {
    }

    /// What `events` come to once chords are taken out.
    struct Result {
        /// The events that are not part of a chord, in order.
        std::vector<gamepad::Event> events;
        /// The actions the chords performed, in order.
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

    const Shortcuts* shortcuts_;
    std::optional<LongHold> longHold_;
    /// One gate per `Event::source`.
    std::map<std::string, gamepad::ChordGate> gates_;
};

} // namespace opensu::input
