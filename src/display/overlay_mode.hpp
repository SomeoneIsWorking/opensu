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
};

} // namespace opensu::display
