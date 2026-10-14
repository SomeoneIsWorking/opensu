#include "pad_chords.hpp"

namespace opensu::input {

gamepad::ChordTriggers chordTriggers(const Shortcuts& shortcuts) {
    gamepad::ChordTriggers triggers{{}, holdDelay};
    for (const Action action : allActions) {
        if (const std::optional<PadChord> chord = shortcuts.padChord(action)) {
            triggers.chords.insert(*chord);
        }
    }
    return triggers;
}

PadChords::Result PadChords::feed(const std::vector<gamepad::Event>& events, Clock::time_point at) {
    Result result;
    const gamepad::ChordTriggers triggers = chordTriggers(*shortcuts_);
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
            if (step.kind == gamepad::ChordGate::Step::Kind::Chord) {
                if (const std::optional<Action> action = shortcuts_->actionFor(step.chord)) {
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

} // namespace opensu::input
