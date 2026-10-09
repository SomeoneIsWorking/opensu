#include "pad_translator.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

#include <linux/input-event-codes.h>

namespace opensu::gamepad {
namespace {

/// Face, shoulder, menu and stick buttons the virtual pad carries, with opensu's name for each.
/// BTN_NORTH and BTN_WEST are BTN_X and BTN_Y, as xpad reports the Xbox X and Y.
constexpr std::pair<std::uint16_t, Button> forwardedKeys[] = {
    {BTN_SOUTH, Button::A},       {BTN_EAST, Button::B},      {BTN_NORTH, Button::X},
    {BTN_WEST, Button::Y},        {BTN_TL, Button::L1},       {BTN_TR, Button::R1},
    {BTN_SELECT, Button::Select}, {BTN_START, Button::Start}, {BTN_THUMBL, Button::L3},
    {BTN_THUMBR, Button::R3},
};

/// How far the left stick must move before it counts as a direction, as the shell's reader uses.
constexpr float stickThreshold = 0.5f;

bool isStick(std::uint16_t code) {
    return code == ABS_X || code == ABS_Y || code == ABS_RX || code == ABS_RY;
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

void PadTranslator::translate(const PadEvent& in, std::vector<PadEvent>& forward,
                              std::vector<Event>& controls) {
    if (in.type == EV_SYN && in.code == SYN_REPORT) {
        forward.push_back(in);
        return;
    }
    if (in.type == EV_KEY) {
        // 2 is autorepeat, which a pad never needs forwarded or reported.
        if (in.value == 2) {
            return;
        }
        const bool pressed = in.value != 0;
        if (in.code == BTN_MODE) {
            control(Button::Guide, pressed, controls);
            return;
        }
        if (in.code >= BTN_DPAD_UP && in.code <= BTN_DPAD_RIGHT) {
            // A d-pad reported as buttons becomes the hat the virtual pad has.
            dpad_[in.code] = pressed;
            const auto held = [this](std::uint16_t code) {
                const auto found = dpad_.find(code);
                return found != dpad_.end() && found->second;
            };
            const bool vertical = in.code == BTN_DPAD_UP || in.code == BTN_DPAD_DOWN;
            const std::uint16_t hat = vertical ? ABS_HAT0Y : ABS_HAT0X;
            const std::int32_t value =
                vertical ? (held(BTN_DPAD_DOWN) ? 1 : 0) - (held(BTN_DPAD_UP) ? 1 : 0)
                         : (held(BTN_DPAD_RIGHT) ? 1 : 0) - (held(BTN_DPAD_LEFT) ? 1 : 0);
            forwardAxis(hat, value, forward);
            hatControls({EV_ABS, hat, value}, controls);
            return;
        }
        if ((in.code == BTN_TL2 && !physical_.axes.contains(ABS_Z) &&
             !physical_.axes.contains(ABS_BRAKE)) ||
            (in.code == BTN_TR2 && !physical_.axes.contains(ABS_RZ) &&
             !physical_.axes.contains(ABS_GAS))) {
            // A digital trigger drives the analogue one fully.
            forwardAxis(in.code == BTN_TL2 ? ABS_Z : ABS_RZ, pressed ? virtualTrigger.max : 0,
                        forward);
            control(in.code == BTN_TL2 ? Button::L2 : Button::R2, pressed, controls);
            return;
        }
        for (const auto& [code, button] : forwardedKeys) {
            if (code == in.code) {
                forwardKey(code, pressed ? 1 : 0, forward);
                control(button, pressed, controls);
                return;
            }
        }
        return;
    }
    if (in.type == EV_ABS) {
        const auto range = physical_.axes.find(in.code);
        if (range == physical_.axes.end()) {
            return;
        }
        if (isStick(in.code)) {
            forwardAxis(in.code, rescale(in.value, range->second, virtualStick), forward);
            stickControls(in, controls);
            return;
        }
        if (in.code == ABS_Z || in.code == ABS_RZ || in.code == ABS_BRAKE || in.code == ABS_GAS) {
            forwardAxis(triggerAxis(in.code), rescale(in.value, range->second, virtualTrigger),
                        forward);
            return;
        }
        if (in.code == ABS_HAT0X || in.code == ABS_HAT0Y) {
            const std::int32_t value = std::clamp(in.value, -1, 1);
            forwardAxis(in.code, value, forward);
            hatControls({EV_ABS, in.code, value}, controls);
        }
    }
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
    for (const auto& [button, on] : {std::pair{negative, centred < -stickThreshold},
                                     std::pair{positive, centred > stickThreshold}}) {
        bool& held = stickHeld_[button];
        if (held != on) {
            held = on;
            control(button, on, controls);
        }
    }
}

} // namespace opensu::gamepad
