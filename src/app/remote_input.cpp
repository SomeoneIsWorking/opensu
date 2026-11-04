#include "remote_input.hpp"

#include <algorithm>
#include <cmath>

namespace opensu::app {
namespace {

using gamepad::Button;

std::uint32_t bit(Button button) {
    return 1U << static_cast<unsigned>(button);
}

/// A full trigger, and a stick at the end of its travel, on the virtual pad's scales.
constexpr std::uint8_t fullTrigger = 255;
constexpr std::int16_t fullTravel = 32767;
constexpr double pointerScale = 65535.0;

} // namespace

void RemoteInput::button(const gamepad::Event& event) {
    if (event.kind != gamepad::Event::Kind::Button || event.button == Button::Guide ||
        event.button == Button::None || event.button == Button::Search) {
        return;
    }
    std::uint32_t& held = event.stick ? stickHeld_ : pad_.buttons;
    held = event.pressed ? held | bit(event.button) : held & ~bit(event.button);
    if (event.button == Button::L2) {
        pad_.leftTrigger = event.pressed ? fullTrigger : 0;
    } else if (event.button == Button::R2) {
        pad_.rightTrigger = event.pressed ? fullTrigger : 0;
    }
    if (!padSticks_) {
        const auto travel = [this](Button less, Button more) {
            const int value = ((stickHeld_ & bit(more)) != 0 ? 1 : 0) -
                              ((stickHeld_ & bit(less)) != 0 ? 1 : 0);
            return static_cast<std::int16_t>(value * fullTravel);
        };
        pad_.leftX = travel(Button::Left, Button::Right);
        pad_.leftY = travel(Button::Up, Button::Down);
    }
}

void RemoteInput::sticks(const std::map<std::string, gamepad::Sticks>& sticks) {
    padSticks_ = !sticks.empty();
    if (!padSticks_) {
        return;
    }
    const gamepad::Sticks& first = sticks.begin()->second;
    const auto narrow = [](std::int32_t value) {
        return static_cast<std::int16_t>(std::clamp(value, -32768, 32767));
    };
    pad_.leftX = narrow(first.leftX);
    pad_.leftY = narrow(first.leftY);
    pad_.rightX = narrow(first.rightX);
    pad_.rightY = narrow(first.rightY);
}

void RemoteInput::pointer(const PointerFrame& frame, const display::Placed& picture) {
    if (frame.point && picture.width > 1 && picture.height > 1) {
        const auto across = [](float at, int start, int size) {
            const double share = (static_cast<double>(at) - start) / (size - 1);
            return static_cast<std::uint16_t>(std::lround(std::clamp(share, 0.0, 1.0) * pointerScale));
        };
        mouse_.x = across(frame.point->x, picture.x, picture.width);
        mouse_.y = across(frame.point->y, picture.y, picture.height);
    }
    for (const bool down : frame.primary) {
        mouse_.buttons = down ? mouse_.buttons | 1U : mouse_.buttons & ~1U;
    }
    // A right or middle press comes as one; it is held for the frame it came in.
    mouse_.buttons = static_cast<std::uint8_t>((mouse_.buttons & 1U) | (frame.middle ? 2U : 0U) |
                                               (frame.right ? 4U : 0U));
    mouse_.wheel += static_cast<std::int32_t>(std::lround(frame.wheel));
}

void RemoteInput::release() {
    pad_ = stream::wire::PadState{};
    stickHeld_ = 0;
    mouse_.buttons = 0;
}

} // namespace opensu::app
