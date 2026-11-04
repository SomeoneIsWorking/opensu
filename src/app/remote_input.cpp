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

std::int16_t narrowed(std::int32_t value) {
    return static_cast<std::int16_t>(std::clamp(value, -32768, 32767));
}

std::uint8_t travel(std::int32_t value) {
    return static_cast<std::uint8_t>(std::clamp<std::int32_t>(value, 0, fullTrigger));
}

/// The pointer buttons `frame` saw go down, bit 0 left, 1 middle, 2 right.
std::uint8_t clicks(const PointerFrame& frame) {
    return static_cast<std::uint8_t>((frame.left ? 1U : 0U) | (frame.middle ? 2U : 0U) |
                                     (frame.right ? 4U : 0U));
}

} // namespace

void RemoteInput::button(const gamepad::Event& event) {
    if (event.kind != gamepad::Event::Kind::Button || event.button == Button::Guide ||
        event.button == Button::None || event.button == Button::Search) {
        return;
    }
    if (event.stick) {
        stickHeld_ =
            event.pressed ? stickHeld_ | bit(event.button) : stickHeld_ & ~bit(event.button);
        return;
    }
    if (event.pressed) {
        held_ |= bit(event.button);
        pressedThisFrame_ |= bit(event.button);
        return;
    }
    held_ &= ~bit(event.button);
    if ((pressedThisFrame_ & bit(event.button)) != 0) {
        endedThisFrame_ |= bit(event.button);
    }
}

void RemoteInput::axes(const std::map<std::string, gamepad::PadAxes>& axes) {
    axes_ = axes.empty() ? std::nullopt : std::optional{axes.begin()->second};
}

void RemoteInput::pointer(const PointerFrame& frame, const display::Placed& picture) {
    if (frame.point && picture.width > 1 && picture.height > 1) {
        const auto across = [](float at, int start, int size) {
            const double share = (static_cast<double>(at) - start) / (size - 1);
            return static_cast<std::uint16_t>(
                std::lround(std::clamp(share, 0.0, 1.0) * pointerScale));
        };
        mouse_.x = across(frame.point->x, picture.x, picture.width);
        mouse_.y = across(frame.point->y, picture.y, picture.height);
    }
    mouse_.buttons = frame.held;
    clicked_ = static_cast<std::uint8_t>(clicked_ | clicks(frame));
    wheelRest_ += frame.wheel;
    const float notches = std::trunc(wheelRest_);
    mouse_.wheel += static_cast<std::int32_t>(notches);
    wheelRest_ -= notches;
}

RemoteInput::State RemoteInput::take() {
    State state;
    state.pad.buttons = held_ | endedThisFrame_;
    if (axes_) {
        state.pad.leftX = narrowed(axes_->sticks.leftX);
        state.pad.leftY = narrowed(axes_->sticks.leftY);
        state.pad.rightX = narrowed(axes_->sticks.rightX);
        state.pad.rightY = narrowed(axes_->sticks.rightY);
        state.pad.leftTrigger = travel(axes_->leftTrigger);
        state.pad.rightTrigger = travel(axes_->rightTrigger);
    } else {
        const auto stick = [this](Button less, Button more) {
            const int value =
                ((stickHeld_ & bit(more)) != 0 ? 1 : 0) - ((stickHeld_ & bit(less)) != 0 ? 1 : 0);
            return static_cast<std::int16_t>(value * fullTravel);
        };
        state.pad.leftX = stick(Button::Left, Button::Right);
        state.pad.leftY = stick(Button::Up, Button::Down);
        state.pad.leftTrigger = (state.pad.buttons & bit(Button::L2)) != 0 ? fullTrigger : 0;
        state.pad.rightTrigger = (state.pad.buttons & bit(Button::R2)) != 0 ? fullTrigger : 0;
    }
    state.mouse = mouse_;
    state.mouse.buttons = static_cast<std::uint8_t>(mouse_.buttons | clicked_);
    pressedThisFrame_ = 0;
    endedThisFrame_ = 0;
    clicked_ = 0;
    return state;
}

void RemoteInput::release() {
    held_ = 0;
    pressedThisFrame_ = 0;
    endedThisFrame_ = 0;
    stickHeld_ = 0;
    axes_.reset();
    mouse_.buttons = 0;
    clicked_ = 0;
}

} // namespace opensu::app
