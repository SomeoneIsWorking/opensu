// touch_surface — what an on-screen pad works with, whichever window it is in: a finger, the
// surface it covers and the overlay it draws for it.
#pragma once

#include <algorithm>
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

/// A rectangle on the target, in pixels.
struct Box {
    float x{0.0F};
    float y{0.0F};
    float width{0.0F};
    float height{0.0F};

    [[nodiscard]] float centreX() const {
        return x + (width * 0.5F);
    }
    [[nodiscard]] float centreY() const {
        return y + (height * 0.5F);
    }
    [[nodiscard]] bool contains(float px, float py) const {
        return px >= x && py >= y && px < x + width && py < y + height;
    }
    [[nodiscard]] Box grown(float by) const {
        return {x - by, y - by, width + (2.0F * by), height + (2.0F * by)};
    }
    bool operator==(const Box&) const = default;
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

    /// A portrait target: the controls go under the picture rather than beside it.
    [[nodiscard]] bool upright() const {
        return height > width;
    }
    /// `dp`, less if controls `width` by `height` dp would run off the safe area at it.
    [[nodiscard]] float dpWithin(float width, float height) const {
        return std::min({dp, safeWidth / width, safeHeight / height});
    }
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
