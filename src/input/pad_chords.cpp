#include "pad_chords.hpp"

namespace opensu::input {

PadChords::Result PadChords::feed(const std::vector<gamepad::Event>& events) {
    Result result;
    for (const gamepad::Event& event : events) {
        if (event.kind != gamepad::Event::Kind::Button) {
            result.events.push_back(event);
            continue;
        }
        if (event.button == gamepad::Button::Guide) {
            holdGuide(event, result);
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
        gamepad::Button via = gamepad::Button::None;
        for (const gamepad::Button modifier : held_) {
            if (canHold(modifier) && !chord) {
                chord = shortcuts_->actionFor(PadChord{modifier, event.button});
                via = modifier;
            }
        }
        held_.insert(event.button);
        if (chord) {
            swallowed_.insert(event.button);
            result.actions.push_back(*chord);
            if (via == gamepad::Button::Guide) {
                guide_.reset();
            }
        } else {
            result.events.push_back(event);
        }
    }
    return result;
}

void PadChords::holdGuide(const gamepad::Event& event, Result& result) {
    if (event.pressed) {
        held_.insert(event.button);
        guide_ = event;
        return;
    }
    held_.erase(event.button);
    if (guide_) {
        result.events.push_back(*guide_);
        result.events.push_back(event);
        guide_.reset();
    }
}

} // namespace opensu::input
