#include "chord_gate.hpp"

#include <algorithm>
#include <utility>

namespace opensu::gamepad {
namespace {

ChordGate::Step edge(Button button, bool pressed) {
    return {ChordGate::Step::Kind::Edge, button, pressed, Chord{}};
}

} // namespace

bool ChordTriggers::isModifier(Button button) const {
    return std::ranges::any_of(chords, [button](const Chord& chord) {
        return chord.modifier == button;
    });
}

void ChordGate::setTriggers(ChordTriggers triggers) {
    triggers_ = std::move(triggers);
}

std::vector<ChordGate::Step> ChordGate::feed(Button button, bool pressed, Clock::time_point at) {
    return pressed ? press(button, at) : release(button, at);
}

bool ChordGate::passes(Button button) const {
    if (swallowed_.contains(button)) {
        return false;
    }
    if (!triggers_.isModifier(button)) {
        return true;
    }
    const auto held = modifiers_.find(button);
    return held != modifiers_.end() && held->second.hold == Hold::LetOut;
}

std::vector<ChordGate::Step> ChordGate::press(Button button, Clock::time_point at) {
    if (modifiers_.contains(button) || swallowed_.contains(button)) {
        return {};
    }
    for (auto& [modifier, held] : modifiers_) {
        const Chord chord{modifier, button};
        if (held.hold != Hold::LetOut && triggers_.chords.contains(chord)) {
            held.hold = Hold::Chorded;
            swallowed_.insert(button);
            return {Step{Step::Kind::Chord, Button::None, false, chord}};
        }
    }
    std::vector<Step> steps;
    for (auto& [modifier, held] : modifiers_) {
        if (held.hold == Hold::Pending) {
            held.hold = Hold::LetOut;
            steps.push_back(edge(modifier, true));
        }
    }
    if (triggers_.isModifier(button)) {
        modifiers_[button] = Held{Hold::Pending, at};
    } else {
        steps.push_back(edge(button, true));
    }
    return steps;
}

std::vector<ChordGate::Step> ChordGate::release(Button button, Clock::time_point at) {
    if (swallowed_.erase(button) != 0) {
        return {};
    }
    const auto found = modifiers_.find(button);
    if (found == modifiers_.end()) {
        return {edge(button, false)};
    }
    const Held held = found->second;
    modifiers_.erase(found);
    switch (held.hold) {
    case Hold::Pending:
        if (at - held.since < triggers_.holdDelay) {
            return {edge(button, true), edge(button, false)};
        }
        return {};
    case Hold::Chorded:
        return {};
    case Hold::LetOut:
        break;
    }
    return {edge(button, false)};
}

} // namespace opensu::gamepad
