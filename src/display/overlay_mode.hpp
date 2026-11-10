// overlay_mode — what openSU's window shows over a running game, whichever window system draws it.
#pragma once

#include <cstdint>

namespace opensu::display {

enum class OverlayMode : std::uint8_t {
    /// Nothing; the game has the input.
    Hidden,
    /// Drawn, and the game keeps the keyboard and the pointer.
    Passive,
    /// Drawn, with the keyboard.
    Interactive,
    /// The on-screen keyboard drawn over the game, which keeps the keyboard it is typed into.
    Typing,
};

/// Whether the shell takes the game's pads: a menu, a page or the on-screen keyboard is up.
[[nodiscard]] constexpr bool takesPads(OverlayMode mode) noexcept {
    return mode == OverlayMode::Interactive || mode == OverlayMode::Typing;
}

} // namespace opensu::display
