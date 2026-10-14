// chord_capture — listening for a pad chord, as the shortcut editor and the mapping editor both do:
// the first button held is the modifier and the next button pressed while it is held the partner.
// B pressed and released with nothing else held cancels, so B can still be held as a modifier.
#pragma once

#include <optional>
#include <vector>

#include "gamepad/event.hpp"
#include "input/shortcuts.hpp"

namespace opensu::app {

class ChordCapture {
  public:
    /// What an edge came to: a chord, or B alone to cancel.
    struct Heard {
        bool cancelled{false};
        input::PadChord chord;
    };

    /// Starts listening with nothing held.
    void begin();
    /// Whether a button is held now.
    [[nodiscard]] bool holding() const noexcept {
        return !held_.empty();
    }
    /// Hears one button edge; a chord once a partner is pressed, the cancel on B's lone release.
    [[nodiscard]] std::optional<Heard> feed(const gamepad::Event& event);

  private:
    /// The buttons held, in the order they went down; the first is a chord's modifier.
    std::vector<gamepad::Button> held_;
    /// B went down with nothing held: its release alone cancels.
    bool loneB_{false};
};

} // namespace opensu::app
