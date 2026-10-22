#include "pad_translator.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

#include <linux/input-event-codes.h>

namespace opensu::gamepad {
namespace {

/// Face, shoulder, menu and stick buttons the virtual pad carries, with opensu's name for each.
/// BTN_NORTH and BTN_WEST are BTN_X and BTN_Y, as xpad reports the Xbox X and Y.
using KeyButton = std::pair<std::uint16_t, Button>;
constexpr KeyButton forwardedKeys[] = {
    {BTN_SOUTH, Button::A},       {BTN_EAST, Button::B},      {BTN_NORTH, Button::X},
    {BTN_WEST, Button::Y},        {BTN_TL, Button::L1},       {BTN_TR, Button::R1},
    {BTN_SELECT, Button::Select}, {BTN_START, Button::Start}, {BTN_THUMBL, Button::L3},
    {BTN_THUMBR, Button::R3},
};

bool isStick(std::uint16_t code) {
    return code == ABS_X || code == ABS_Y || code == ABS_RX || code == ABS_RY;
}

/// The member of `sticks` that stick axis `code` sets.
std::int32_t& stickAxis(Sticks& sticks, std::uint16_t code) {
    switch (code) {
    case ABS_X:
        return sticks.leftX;
    case ABS_Y:
        return sticks.leftY;
    case ABS_RX:
        return sticks.rightX;
    default:
        return sticks.rightY;
    }
}

/// The virtual trigger axis a physical trigger axis drives; older Xbox One firmware reports
/// triggers as brake and gas.
std::uint16_t triggerAxis(std::uint16_t code) {
    return code == ABS_BRAKE ? ABS_Z : code == ABS_GAS ? ABS_RZ : code;
}

std::int32_t rescale(std::int32_t value, AxisRange from, AxisRange to) {
    if (from.max <= from.min) {
        return to.min;
    }
    const double t = static_cast<double>(std::clamp(value, from.min, from.max) - from.min) /
                     (from.max - from.min);
    return static_cast<std::int32_t>(std::lround(to.min + t * (to.max - to.min)));
}

} // namespace

bool PadCapabilities::isGamepad() const {
    return keys.contains(BTN_SOUTH) && axes.contains(ABS_X) && axes.contains(ABS_Y);
}

PadTranslator::PadTranslator(PadCapabilities physical) : physical_{std::move(physical)} {
}

void PadTranslator::setChords(ChordTriggers chords) {
    chords_ = std::move(chords);
    applyTriggers();
}

void PadTranslator::setForwardMap(ForwardMap map) {
    map_ = std::move(map);
    chordHeld_.clear();
    applyTriggers();
    // The state follows the new map; the virtual pad takes it up on its next resume.
    std::vector<PadEvent> unsent;
    syncOutputs({padButtons.begin(), padButtons.end()}, unsent);
}

void PadTranslator::applyTriggers() {
    ChordTriggers triggers = chords_;
    for (const auto& [chord, buttons] : map_.chords) {
        triggers.chords.insert(chord);
    }
    gate_.setTriggers(std::move(triggers));
}

void PadTranslator::translate(const PadEvent& in, std::vector<PadEvent>& forward,
                              std::vector<Event>& controls, ChordGate::Clock::time_point at) {
    if (in.type == EV_SYN && in.code == SYN_REPORT) {
        forward.push_back(in);
        return;
    }
    std::vector<Event> edges;
    if (in.type == EV_KEY) {
        // 2 is autorepeat, which a pad never needs forwarded or reported.
        if (in.value == 2) {
            return;
        }
        const bool pressed = in.value != 0;
        if (in.code == BTN_MODE) {
            control(Button::Guide, pressed, edges);
        } else if (in.code >= BTN_DPAD_UP && in.code <= BTN_DPAD_RIGHT) {
            // A d-pad reported as buttons becomes the hat the virtual pad has.
            dpad_[in.code] = pressed;
            const auto held = [this](std::uint16_t code) {
                const auto found = dpad_.find(code);
                return found != dpad_.end() && found->second;
            };
            const bool vertical = in.code == BTN_DPAD_UP || in.code == BTN_DPAD_DOWN;
            const std::int32_t value =
                vertical ? (held(BTN_DPAD_DOWN) ? 1 : 0) - (held(BTN_DPAD_UP) ? 1 : 0)
                         : (held(BTN_DPAD_RIGHT) ? 1 : 0) - (held(BTN_DPAD_LEFT) ? 1 : 0);
            const std::uint16_t hat = vertical ? ABS_HAT0Y : ABS_HAT0X;
            hatControls({EV_ABS, hat, value}, edges);
        } else if ((in.code == BTN_TL2 && !physical_.axes.contains(ABS_Z) &&
                    !physical_.axes.contains(ABS_BRAKE)) ||
                   (in.code == BTN_TR2 && !physical_.axes.contains(ABS_RZ) &&
                    !physical_.axes.contains(ABS_GAS))) {
            // A digital trigger drives the analogue one fully.
            const Button button = in.code == BTN_TL2 ? Button::L2 : Button::R2;
            triggers_[button] = pressed ? virtualTrigger.max : 0;
            triggerHeld_[button] = pressed;
            control(button, pressed, edges);
        } else if (const auto key = std::ranges::find(forwardedKeys, in.code, &KeyButton::first);
                   key != std::end(forwardedKeys)) {
            keys_[in.code] = pressed;
            control(key->second, pressed, edges);
        }
    } else if (in.type == EV_ABS) {
        const auto range = physical_.axes.find(in.code);
        if (range == physical_.axes.end()) {
            return;
        }
        if (isStick(in.code)) {
            const std::int32_t value = rescale(in.value, range->second, virtualStick);
            stickAxis(sticks_, in.code) = value;
            forwardAxis(in.code, value, forward);
            stickControls(in, edges);
        } else if (in.code == ABS_Z || in.code == ABS_RZ || in.code == ABS_BRAKE ||
                   in.code == ABS_GAS) {
            const Button button = triggerAxis(in.code) == ABS_Z ? Button::L2 : Button::R2;
            const std::int32_t value = rescale(in.value, range->second, virtualTrigger);
            triggers_[button] = value;
            const float travel = static_cast<float>(value) / virtualTrigger.max;
            bool& held = triggerHeld_[button];
            if (held ? travel <= triggerReleaseAt : travel >= triggerPressAt) {
                held = !held;
                control(button, held, edges);
            } else {
                sync(button, forward);
            }
        } else if (in.code == ABS_HAT0X || in.code == ABS_HAT0Y) {
            hatControls({EV_ABS, in.code, std::clamp(in.value, -1, 1)}, edges);
        }
    }
    gate(edges, forward, controls, at);
}

void PadTranslator::gate(const std::vector<Event>& edges, std::vector<PadEvent>& forward,
                         std::vector<Event>& controls, ChordGate::Clock::time_point at) {
    for (const Event& edge : edges) {
        // opensu hears every edge; it plays the chords from them with its own gate.
        controls.push_back(edge);
        for (const ChordGate::Step& step : gate_.feed(edge.button, edge.pressed, at)) {
            if (step.kind == ChordGate::Step::Kind::Chord) {
                holdChord(step.chord, forward);
            } else if (step.kind != ChordGate::Step::Kind::Edge) {
                continue;
            } else if (step.button == edge.button && step.pressed && !edge.pressed) {
                tap(step.button, forward);
            } else {
                sync(step.button, forward);
            }
        }
        if (const auto chord = chordHeld_.find(edge.button);
            !edge.pressed && chord != chordHeld_.end()) {
            const std::set<Button> outputs = std::move(chord->second);
            chordHeld_.erase(chord);
            syncOutputs(outputs, forward);
        }
        sync(edge.button, forward);
    }
}

void PadTranslator::sync(Button button, std::vector<PadEvent>& forward) {
    syncOutputs(map_.outputOf(button), forward);
}

void PadTranslator::syncOutputs(const std::set<Button>& outputs, std::vector<PadEvent>& forward) {
    const auto on = [this](Button button) {
        return level(button) > 0 ? 1 : 0;
    };
    for (const Button out : outputs) {
        if (out == Button::L2 || out == Button::R2) {
            forwardAxis(out == Button::L2 ? ABS_Z : ABS_RZ, level(out), forward);
        } else if (out == Button::Up || out == Button::Down) {
            forwardAxis(ABS_HAT0Y, on(Button::Down) - on(Button::Up), forward);
        } else if (out == Button::Left || out == Button::Right) {
            forwardAxis(ABS_HAT0X, on(Button::Right) - on(Button::Left), forward);
        } else if (const auto key = std::ranges::find(forwardedKeys, out, &KeyButton::second);
                   key != std::end(forwardedKeys)) {
            forwardKey(key->first, on(out), forward);
        }
    }
}

void PadTranslator::tap(Button button, std::vector<PadEvent>& forward) {
    tapping_ = map_.outputOf(button);
    tapping_.erase(Button::Guide);
    if (tapping_.empty()) {
        return;
    }
    syncOutputs(tapping_, forward);
    tapping_.clear();
    // The release follows in a report of its own, so a game that polls sees the press.
    forward.push_back(PadEvent{EV_SYN, SYN_REPORT, 0});
}

void PadTranslator::holdChord(const Chord& chord, std::vector<PadEvent>& forward) {
    const auto found = map_.chords.find(chord);
    if (found == map_.chords.end()) {
        return;
    }
    chordHeld_[chord.trigger] = found->second;
    syncOutputs(found->second, forward);
}

bool PadTranslator::held(Button button) const {
    const auto in = [button](const std::map<Button, bool>& states) {
        const auto found = states.find(button);
        return found != states.end() && found->second;
    };
    if (button == Button::L2 || button == Button::R2) {
        return in(triggerHeld_);
    }
    if (button == Button::Up || button == Button::Down || button == Button::Left ||
        button == Button::Right) {
        return in(hatHeld_);
    }
    const auto key = std::ranges::find(forwardedKeys, button, &KeyButton::second);
    if (key == std::end(forwardedKeys)) {
        return false;
    }
    const auto found = keys_.find(key->first);
    return found != keys_.end() && found->second;
}

std::int32_t PadTranslator::level(Button out) const {
    const bool trigger = out == Button::L2 || out == Button::R2;
    const std::int32_t full = trigger ? virtualTrigger.max : 1;
    const bool chorded = std::ranges::any_of(chordHeld_, [out](const auto& chord) {
        return chord.second.contains(out);
    });
    if (tapping_.contains(out) || chorded) {
        return full;
    }
    std::int32_t value = 0;
    for (const Button source : padButtons) {
        if (!map_.sends(source, out) || !gate_.passes(source)) {
            continue;
        }
        if (trigger && (source == Button::L2 || source == Button::R2)) {
            // A trigger sent to a trigger keeps its travel.
            const auto travel = triggers_.find(source);
            value = std::max(value, travel != triggers_.end() ? travel->second : 0);
        } else if (held(source)) {
            value = full;
        }
    }
    return value;
}

std::vector<PadEvent> PadTranslator::rest() const {
    std::vector<PadEvent> out;
    for (const auto& [key, value] : state_) {
        if (value != 0) {
            out.push_back(PadEvent{key.first, key.second, 0});
        }
    }
    out.push_back(PadEvent{EV_SYN, SYN_REPORT, 0});
    return out;
}

std::vector<PadEvent> PadTranslator::resume() const {
    std::vector<PadEvent> out;
    for (const auto& [key, value] : state_) {
        if (key.first == EV_ABS && value != 0) {
            out.push_back(PadEvent{key.first, key.second, value});
        }
    }
    out.push_back(PadEvent{EV_SYN, SYN_REPORT, 0});
    return out;
}

void PadTranslator::forwardKey(std::uint16_t code, std::int32_t value,
                               std::vector<PadEvent>& forward) {
    auto& slot = state_[{EV_KEY, code}];
    if (slot == value) {
        return;
    }
    slot = value;
    forward.push_back(PadEvent{EV_KEY, code, value});
}

void PadTranslator::forwardAxis(std::uint16_t code, std::int32_t value,
                                std::vector<PadEvent>& forward) {
    auto& slot = state_[{EV_ABS, code}];
    if (slot == value) {
        return;
    }
    slot = value;
    forward.push_back(PadEvent{EV_ABS, code, value});
}

void PadTranslator::control(Button button, bool pressed, std::vector<Event>& controls) {
    Event event;
    event.kind = Event::Kind::Button;
    event.button = button;
    event.pressed = pressed;
    controls.push_back(event);
}

void PadTranslator::hatControls(const PadEvent& axis, std::vector<Event>& controls) {
    const std::int32_t value = axis.value;
    const bool vertical = axis.code == ABS_HAT0Y;
    const Button negative = vertical ? Button::Up : Button::Left;
    const Button positive = vertical ? Button::Down : Button::Right;
    for (const auto& [button, on] :
         {std::pair{negative, value < 0}, std::pair{positive, value > 0}}) {
        bool& held = hatHeld_[button];
        if (held != on) {
            held = on;
            control(button, on, controls);
            controls.back().stick = true;
        }
    }
}

void PadTranslator::stickControls(const PadEvent& axis, std::vector<Event>& controls) {
    const std::uint16_t code = axis.code;
    const std::int32_t value = axis.value;
    if (code != ABS_X && code != ABS_Y) {
        return;
    }
    const AxisRange range = physical_.axes.at(code);
    const float half = static_cast<float>(range.max - range.min) * 0.5f;
    const float centred =
        half > 0.0f ? (static_cast<float>(value - range.min) - half) / half : 0.0f;
    const bool vertical = code == ABS_Y;
    const Button negative = vertical ? Button::Up : Button::Left;
    const Button positive = vertical ? Button::Down : Button::Right;
    for (const auto& [button, on] : {std::pair{negative, centred < -stickDirectionAt},
                                     std::pair{positive, centred > stickDirectionAt}}) {
        bool& held = stickHeld_[button];
        if (held != on) {
            held = on;
            control(button, on, controls);
            controls.back().stick = true;
        }
    }
}

} // namespace opensu::gamepad
