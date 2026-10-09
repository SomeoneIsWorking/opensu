#include "event.hpp"

#include <array>
#include <utility>

namespace opensu::gamepad {

namespace {

constexpr std::array<std::pair<std::string_view, Button>, 18> names{{
    {"a", Button::A},
    {"b", Button::B},
    {"x", Button::X},
    {"y", Button::Y},
    {"l1", Button::L1},
    {"r1", Button::R1},
    {"l2", Button::L2},
    {"r2", Button::R2},
    {"l3", Button::L3},
    {"r3", Button::R3},
    {"select", Button::Select},
    {"start", Button::Start},
    {"guide", Button::Guide},
    {"up", Button::Up},
    {"down", Button::Down},
    {"left", Button::Left},
    {"right", Button::Right},
    {"search", Button::Search},
}};

} // namespace

std::string_view name(Button button) {
    for (const auto& [spelling, named] : names) {
        if (named == button) {
            return spelling;
        }
    }
    return {};
}

Button buttonNamed(std::string_view text) noexcept {
    for (const auto& [spelling, named] : names) {
        if (spelling == text) {
            return named;
        }
    }
    return Button::None;
}

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
    case Button::Search:
        return "SEARCH";
    case Button::None:
        break;
    }
    return {};
}

} // namespace opensu::gamepad
