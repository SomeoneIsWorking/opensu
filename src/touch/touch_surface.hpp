// touch_surface — what an on-screen pad works with, whichever window it is in: a finger, the
// surface it covers and the overlay it draws for it.
#pragma once

#include <cstdint>
#include <span>

namespace opensu::touch {

/// A finger on the touchscreen, in the target's pixels.
struct Finger {
    enum class Phase : std::uint8_t {
        Down,
        Move,
        Up,
    };

    std::uint64_t id{0};
    float x{0.0F};
    float y{0.0F};
    Phase phase{Phase::Down};
};

/// The target the overlay covers, in pixels: its size, the part no notch or rounded corner
/// covers, and pixels per dp.
struct WindowSurface {
    unsigned width{0};
    unsigned height{0};
    float safeX{0.0F};
    float safeY{0.0F};
    float safeWidth{0.0F};
    float safeHeight{0.0F};
    float dp{1.0F};
};

/// The overlay to draw: rows of RGBA, top first, the size of the target. `changed` when it differs
/// from the last one drawn.
struct OverlayImage {
    std::span<const std::uint8_t> rgba;
    unsigned width{0};
    unsigned height{0};
    bool changed{true};
};

} // namespace opensu::touch
