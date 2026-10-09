#include "icon_size.hpp"

#include <algorithm>

namespace opensu::ui {

int clampIconLevel(int level) noexcept {
    return std::clamp(level, minIconLevel, maxIconLevel);
}

float iconScale(int level) noexcept {
    return std::clamp((static_cast<float>(clampIconLevel(level)) - 10.0f) * 0.11f + 1.45f, 0.67f,
                      2.55f);
}

float relativeIconScale(int level) noexcept {
    return iconScale(level) / iconScale(defaultIconLevel);
}

} // namespace opensu::ui
