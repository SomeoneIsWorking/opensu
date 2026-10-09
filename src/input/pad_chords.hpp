// pad_chords — pad chords told apart from plain button presses. While a chord's modifier (L2, R2,
// L3 or R3) is held, the second button of a chord is an action and not a button press, and its
// release is swallowed too.
#pragma once

#include <set>
#include <vector>

#include "gamepad/event.hpp"
#include "shortcuts.hpp"

namespace opensu::input {

class PadChords {
  public:
    explicit PadChords(const Shortcuts& shortcuts) noexcept : shortcuts_{&shortcuts} {
    }

    /// What `events` come to once chords are taken out.
    struct Result {
        /// The events that are not part of a chord, in order.
        std::vector<gamepad::Event> events;
        /// The actions the chords performed, in order.
        std::vector<Action> actions;
    };

    [[nodiscard]] Result feed(const std::vector<gamepad::Event>& events);

  private:
    const Shortcuts* shortcuts_;
    std::set<gamepad::Button> held_;
    std::set<gamepad::Button> swallowed_;
};

} // namespace opensu::input
