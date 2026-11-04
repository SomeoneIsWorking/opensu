// letterbox — the largest rectangle of an aspect ratio centred in an area, for a picture shown
// whole on a screen of another shape (the runner's frame).
#pragma once

#include <algorithm>
#include <cmath>

namespace opensu::display {

/// A rectangle in whole pixels.
struct Placed {
    int x{0};
    int y{0};
    int width{0};
    int height{0};
};

/// The largest rectangle of `aspect` (width over height) centred in `width` x `height`; all of it
/// when `aspect` is not positive.
[[nodiscard]] inline Placed letterbox(int width, int height, float aspect) noexcept {
    auto drawnWidth = static_cast<float>(width);
    auto drawnHeight = static_cast<float>(height);
    if (aspect > 0.0F) {
        if (drawnWidth / drawnHeight > aspect) {
            drawnWidth = drawnHeight * aspect;
        } else {
            drawnHeight = drawnWidth / aspect;
        }
    }
    Placed placed;
    placed.width = std::max(1, static_cast<int>(std::lround(drawnWidth)));
    placed.height = std::max(1, static_cast<int>(std::lround(drawnHeight)));
    placed.x = (width - placed.width) / 2;
    placed.y = (height - placed.height) / 2;
    return placed;
}

} // namespace opensu::display
