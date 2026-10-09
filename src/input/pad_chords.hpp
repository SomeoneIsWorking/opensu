// pad_chords — pad chords told apart from plain button presses. While a chord's modifier (L2, R2,
// L3 or R3) is held, the second button of a chord is an action and not a button press, and its
// release is swallowed too. Guide is held back until its release: with a chord it is the chord's
// modifier, alone it is a tap that comes out as a press and a release when it is let go.
#pragma once

#include <optional>
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
    /// Guide's press or release: kept back, and let out as a tap when no chord used it.
    void holdGuide(const gamepad::Event& event, Result& result);

    const Shortcuts* shortcuts_;
    std::set<gamepad::Button> held_;
    std::set<gamepad::Button> swallowed_;
    /// Guide's press, kept until its release says whether a chord used it.
    std::optional<gamepad::Event> guide_;
};

} // namespace opensu::input
