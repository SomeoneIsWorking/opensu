// shooter_keys — what the Shooter layout's controls press on a computer: each button a key (as a
// USB HID usage, which SDL scancodes are) or a mouse button, and how far a swipe moves the mouse.
#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "gamepad/event.hpp"

namespace opensu::touch {

/// Keys and mouse buttons held: bit 0 left, 1 middle, 2 right, as `stream::wire::MouseState`.
struct DeskInput {
    std::vector<std::uint8_t> keys;
    std::uint8_t mouseButtons{0};

    bool operator==(const DeskInput&) const = default;
};

/// What the Shooter's `held` buttons press: the move stick WASD, Jump Space, Crouch Left Ctrl,
/// Reload R, Use E, Sprint Left Shift, Q, G, Tab and Escape; Fire the left mouse button and Aim
/// the right. Guide presses nothing.
[[nodiscard]] DeskInput deskInput(std::span<const gamepad::Button> held);

/// Mouse counts for a swipe of one dp across the look area.
inline constexpr float lookCountsPerDp = 4.0F;

} // namespace opensu::touch
