// letterbox — the largest rectangle of an aspect ratio centred in an area, for a picture shown
// whole on a screen of another shape (the runner's frame, a remote game's picture).
#pragma once

#include <algorithm>
#include <cmath>

namespace opensu::display {

/// An area's size in whole pixels.
struct Size {
    int width{0};
    int height{0};
};

/// A rectangle in whole pixels.
struct Placed {
    int x{0};
    int y{0};
    int width{0};
    int height{0};
};

/// The largest rectangle of `aspect` (width over height) centred in `area`; all of it when `aspect`
/// is not positive.
[[nodiscard]] inline Placed letterbox(Size area, float aspect) noexcept {
    auto drawnWidth = static_cast<float>(area.width);
    auto drawnHeight = static_cast<float>(area.height);
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
    placed.x = (area.width - placed.width) / 2;
    placed.y = (area.height - placed.height) / 2;
    return placed;
}

} // namespace opensu::display
