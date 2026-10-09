#include "pad_chords.hpp"

namespace opensu::input {

PadChords::Result PadChords::feed(const std::vector<gamepad::Event>& events) {
    Result result;
    for (const gamepad::Event& event : events) {
        if (event.kind != gamepad::Event::Kind::Button) {
            result.events.push_back(event);
            continue;
        }
        if (!event.pressed) {
            held_.erase(event.button);
            if (swallowed_.erase(event.button) == 0) {
                result.events.push_back(event);
            }
            continue;
        }
        std::optional<Action> chord;
        for (const gamepad::Button modifier : held_) {
            if (canHold(modifier) && !chord) {
                chord = shortcuts_->actionFor(PadChord{modifier, event.button});
            }
        }
        held_.insert(event.button);
        if (chord) {
            swallowed_.insert(event.button);
            result.actions.push_back(*chord);
        } else {
            result.events.push_back(event);
        }
    }
    return result;
}

} // namespace opensu::input
