// letterbox — the largest rectangle of an aspect ratio in an area, for a picture shown whole on a
// screen of another shape (the runner's frame, a remote game's picture).
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

/// A picture of `width` by `height` stood in `area`: centred, except that a wide picture on a tall
/// screen stands at its top, leaving the rest to the touch controls.
[[nodiscard]] inline Placed standIn(Size area, int width, int height) noexcept {
    const bool wideOnTall = width > height && area.height > area.width;
    return Placed{.x = (area.width - width) / 2,
                  .y = wideOnTall ? 0 : (area.height - height) / 2,
                  .width = width,
                  .height = height};
}

/// The largest rectangle of `aspect` (width over height) in `area`, stood there by `standIn`; all
/// of it when `aspect` is not positive.
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
    return standIn(area, std::max(1, static_cast<int>(std::lround(drawnWidth))),
                   std::max(1, static_cast<int>(std::lround(drawnHeight))));
}

/// `letterbox` in `area` less its top `top` rows (a screen's cutout) and bottom `bottom` (the touch
/// controls' band).
[[nodiscard]] inline Placed letterboxBetween(Size area, int top, int bottom,
                                             float aspect) noexcept {
    const int clearTop = std::clamp(top, 0, area.height);
    const int clearBottom = std::clamp(bottom, 0, area.height - clearTop);
    Placed placed =
        letterbox({.width = area.width, .height = area.height - clearTop - clearBottom}, aspect);
    placed.y += clearTop;
    return placed;
}

} // namespace opensu::display
