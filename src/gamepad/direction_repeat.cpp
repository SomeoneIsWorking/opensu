#include "direction_repeat.hpp"

namespace iideck::gamepad {

bool DirectionRepeat::repeats(Button button) {
    return button == Button::Up || button == Button::Down || button == Button::Left ||
           button == Button::Right;
}

void DirectionRepeat::press(Button direction, Clock::time_point now) {
    held_ = direction;
    pending_ = direction;
    next_ = now;
}

void DirectionRepeat::release(Button direction) {
    if (held_ == direction) {
        held_ = Button::None;
    }
}

std::optional<Button> DirectionRepeat::poll(Clock::time_point now) {
    // A press moves once even when it was released before this poll.
    if (pending_ != Button::None) {
        const Button direction = pending_;
        pending_ = Button::None;
        next_ = now + delay;
        return direction;
    }
    if (held_ == Button::None || now < next_) {
        return std::nullopt;
    }
    next_ = now + interval;
    return held_;
}

} // namespace iideck::gamepad
