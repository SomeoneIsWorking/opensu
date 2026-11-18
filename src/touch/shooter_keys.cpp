#include "touch/shooter_keys.hpp"

#include <algorithm>
#include <array>
#include <utility>

namespace opensu::touch {
namespace {

using gamepad::Button;

constexpr std::array<std::pair<Button, std::uint8_t>, 13> keyOf{{
    {Button::Up, 0x1a},
    {Button::Left, 0x04},
    {Button::Down, 0x16},
    {Button::Right, 0x07},
    {Button::A, 0x2c},
    {Button::B, 0xe0},
    {Button::X, 0x15},
    {Button::Y, 0x08},
    {Button::L1, 0x14},
    {Button::R1, 0x0a},
    {Button::L3, 0xe1},
    {Button::Select, 0x2b},
    {Button::Start, 0x29},
}};
constexpr std::uint8_t leftMouse = 1U << 0U;
constexpr std::uint8_t rightMouse = 1U << 2U;

} // namespace

DeskInput deskInput(std::span<const gamepad::Button> held) {
    DeskInput desk;
    for (const Button button : held) {
        if (button == Button::R2) {
            desk.mouseButtons |= leftMouse;
        } else if (button == Button::L2) {
            desk.mouseButtons |= rightMouse;
        } else if (const auto* found =
                       std::ranges::find(keyOf, button, &std::pair<Button, std::uint8_t>::first);
                   found != keyOf.end()) {
            desk.keys.push_back(found->second);
        }
    }
    return desk;
}

} // namespace opensu::touch
