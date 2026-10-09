#include "event.hpp"

namespace opensu::gamepad {

std::string_view prompt(Button button) {
    switch (button) {
    case Button::A:
        return "A";
    case Button::B:
        return "B";
    case Button::X:
        return "X";
    case Button::Y:
        return "Y";
    case Button::L1:
        return "LB";
    case Button::R1:
        return "RB";
    case Button::L2:
        return "LT";
    case Button::R2:
        return "RT";
    case Button::L3:
        return "L3";
    case Button::R3:
        return "R3";
    case Button::Select:
        return "SELECT";
    case Button::Start:
        return "START";
    case Button::Guide:
        return "GUIDE";
    case Button::Up:
        return "UP";
    case Button::Down:
        return "DOWN";
    case Button::Left:
        return "LEFT";
    case Button::Right:
        return "RIGHT";
    case Button::None:
        break;
    }
    return {};
}

} // namespace opensu::gamepad
