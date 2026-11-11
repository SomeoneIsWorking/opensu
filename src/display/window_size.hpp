// window_size — the window an interface sized in dp opens in on a display.
#pragma once

#include <algorithm>
#include <cmath>

#include "letterbox.hpp"

namespace opensu::display {

/// A window of `dp` on a display of `scale` window units per dp (SDL's content scale: 1 where the
/// system sizes windows in points), no larger than `usable`, the display less its panels (zero
/// when unknown).
[[nodiscard]] inline Size windowOn(Size dp, float scale, Size usable) noexcept {
    const float factor = scale > 0.0F ? scale : 1.0F;
    Size window{.width = static_cast<int>(std::lround(static_cast<float>(dp.width) * factor)),
                .height = static_cast<int>(std::lround(static_cast<float>(dp.height) * factor))};
    if (usable.width > 0 && usable.height > 0) {
        window.width = std::min(window.width, usable.width);
        window.height = std::min(window.height, usable.height);
    }
    return window;
}

} // namespace opensu::display
