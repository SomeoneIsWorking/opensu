#include "direction_repeat.hpp"

namespace iideck::gamepad {

bool DirectionRepeat::repeats(Button button) {
    return button == Button::Up || button == Button::Down || button == Button::Left ||
           button == Button::Right;
}

void DirectionRepeat::press(Button direction, Clock::time_point now) {
    held_ = direction;
    next_ = now;
    first_ = true;
}

void DirectionRepeat::release(Button direction) {
    if (held_ == direction) {
        held_ = Button::None;
    }
}

std::optional<Button> DirectionRepeat::poll(Clock::time_point now) {
    if (held_ == Button::None || now < next_) {
        return std::nullopt;
    }
    next_ = now + (first_ ? delay : interval);
    first_ = false;
    return held_;
}

} // namespace iideck::gamepad
