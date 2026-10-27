#include "interface_input.hpp"

#include <array>
#include <utility>

namespace opensu::ui {
namespace {

using gpu2d::ui::Action;

constexpr std::array<std::pair<gamepad::Button, Action>, 11> pairs{{
    {gamepad::Button::Up, Action::Up},
    {gamepad::Button::Down, Action::Down},
    {gamepad::Button::Left, Action::Left},
    {gamepad::Button::Right, Action::Right},
    {gamepad::Button::A, Action::Accept},
    {gamepad::Button::B, Action::Back},
    {gamepad::Button::X, Action::Secondary},
    {gamepad::Button::Y, Action::Tertiary},
    {gamepad::Button::L1, Action::PreviousTab},
    {gamepad::Button::R1, Action::NextTab},
    {gamepad::Button::Start, Action::Menu},
}};

} // namespace

std::optional<Action> interfaceActionOf(gamepad::Button button) noexcept {
    for (const auto& [from, action] : pairs) {
        if (from == button) {
            return action;
        }
    }
    return std::nullopt;
}

gamepad::Button buttonOfInterfaceAction(Action action) noexcept {
    for (const auto& [button, to] : pairs) {
        if (to == action) {
            return button;
        }
    }
    return gamepad::Button::None;
}

} // namespace opensu::ui
