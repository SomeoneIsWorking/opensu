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

std::vector<ChordGate::Step> ChordGate::tick(Clock::time_point at) {
    std::vector<Step> steps;
    for (auto& [modifier, held] : modifiers_) {
        if (held.hold == Hold::Pending && !held.longHeld &&
            at - held.since >= triggers_.holdDelay) {
            held.longHeld = true;
            steps.push_back(Step{Step::Kind::LongHold, modifier, true, Chord{}});
        }
    }
    return steps;
}

void ChordGate::endLongHold(Button modifier, Held& held, std::vector<Step>& steps) {
    if (held.longHeld) {
        held.longHeld = false;
        steps.push_back(Step{Step::Kind::LongHoldEnded, modifier, false, Chord{}});
    }
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
            std::vector<Step> steps;
            endLongHold(modifier, held, steps);
            steps.push_back(Step{Step::Kind::Chord, Button::None, false, chord});
            return steps;
        }
    }
    std::vector<Step> steps;
    for (auto& [modifier, held] : modifiers_) {
        if (held.hold == Hold::Pending) {
            held.hold = Hold::LetOut;
            endLongHold(modifier, held, steps);
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
    Held held = found->second;
    modifiers_.erase(found);
    std::vector<Step> steps;
    switch (held.hold) {
    case Hold::Pending:
        if (at - held.since < triggers_.holdDelay) {
            return {edge(button, true), edge(button, false)};
        }
        endLongHold(button, held, steps);
        break;
    case Hold::Chorded:
        break;
    case Hold::LetOut:
        steps.push_back(edge(button, false));
        break;
    }
    return steps;
}

} // namespace opensu::gamepad
