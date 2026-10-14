// chord_gate — the one rule that tells pad chords from plain presses, for whatever reads a pad's
// edges: the shell's controls (`input::PadChords`) and the game's virtual pad (`PadTranslator`).
//
// A modifier is a button some bound chord starts with. Its press is held back. A press of a bound
// trigger while it is held is the chord: the trigger's press and release and the modifier's release
// are swallowed. A press of any other button means no chord is coming: the modifier's press is let
// out first, then that press. A modifier released alone comes out as a tap when it was held for
// less than the hold delay, and not at all after it. Pure state with the time passed in.
#pragma once

#include <chrono>
#include <compare>
#include <cstdint>
#include <map>
#include <set>
#include <vector>

#include "event.hpp"

namespace opensu::gamepad {

/// A modifier held while a trigger is pressed.
struct Chord {
    Button modifier{Button::None};
    Button trigger{Button::None};

    auto operator<=>(const Chord&) const = default;
};

/// The chords bound to something, and how long a lone modifier may be held and still be a tap.
struct ChordTriggers {
    std::set<Chord> chords;
    std::chrono::milliseconds holdDelay{0};

    [[nodiscard]] bool isModifier(Button button) const;

    bool operator==(const ChordTriggers&) const = default;
};

class ChordGate {
  public:
    using Clock = std::chrono::steady_clock;

    /// What one edge comes to.
    struct Step {
        enum class Kind : std::uint8_t {
            /// `button` goes down or up.
            Edge,
            /// `chord` was played.
            Chord,
        };

        Kind kind{Kind::Edge};
        Button button{Button::None};
        bool pressed{false};
        gamepad::Chord chord;

        bool operator==(const Step&) const = default;
    };

    void setTriggers(ChordTriggers triggers);
    [[nodiscard]] const ChordTriggers& triggers() const noexcept {
        return triggers_;
    }

    /// `button` went down or up at `at`: the edges to pass on and the chords played, in order.
    [[nodiscard]] std::vector<Step> feed(Button button, bool pressed, Clock::time_point at);

    /// Whether `button`'s state passes now: false while it is a modifier not let out, or a
    /// trigger swallowed by a chord.
    [[nodiscard]] bool passes(Button button) const;

  private:
    enum class Hold : std::uint8_t {
        /// Waiting to see whether a chord follows.
        Pending,
        /// Started a chord; its release is swallowed.
        Chorded,
        /// Let out, because another button was pressed.
        LetOut,
    };

    struct Held {
        Hold hold{Hold::Pending};
        Clock::time_point since;
    };

    [[nodiscard]] std::vector<Step> press(Button button, Clock::time_point at);
    [[nodiscard]] std::vector<Step> release(Button button, Clock::time_point at);

    ChordTriggers triggers_;
    std::map<Button, Held> modifiers_;
    std::set<Button> swallowed_;
};

} // namespace opensu::gamepad
