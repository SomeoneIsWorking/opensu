#include "pad_chords.hpp"

namespace opensu::input {

PadChords::Result PadChords::feed(const std::vector<gamepad::Event>& events, Clock::time_point at) {
    Result result;
    const gamepad::ChordTriggers triggers = chords_->triggers();
    for (const gamepad::Event& event : events) {
        if (event.kind != gamepad::Event::Kind::Button) {
            result.events.push_back(event);
            continue;
        }
        gamepad::ChordGate& gate = gates_[event.source];
        if (gate.triggers() != triggers) {
            gate.setTriggers(triggers);
        }
        for (const gamepad::ChordGate::Step& step : gate.feed(event.button, event.pressed, at)) {
            if (noteLongHold(event.source, step)) {
                continue;
            }
            if (step.kind == gamepad::ChordGate::Step::Kind::Chord) {
                if (const std::optional<Action> action = chords_->actionFor(step.chord)) {
                    result.actions.push_back(*action);
                }
                continue;
            }
            gamepad::Event out = event;
            out.button = step.button;
            out.pressed = step.pressed;
            result.events.push_back(out);
        }
    }
    return result;
}

std::optional<gamepad::Button> PadChords::longHeld(Clock::time_point at) {
    for (auto& [source, gate] : gates_) {
        for (const gamepad::ChordGate::Step& step : gate.tick(at)) {
            noteLongHold(source, step);
        }
    }
    if (!longHold_) {
        return std::nullopt;
    }
    return longHold_->modifier;
}

bool PadChords::noteLongHold(const std::string& source, const gamepad::ChordGate::Step& step) {
    using Kind = gamepad::ChordGate::Step::Kind;
    if (step.kind == Kind::LongHold) {
        longHold_ = LongHold{source, step.button};
        return true;
    }
    if (step.kind == Kind::LongHoldEnded) {
        if (longHold_ && longHold_->source == source && longHold_->modifier == step.button) {
            longHold_.reset();
        }
        return true;
    }
    return false;
}

} // namespace opensu::input
