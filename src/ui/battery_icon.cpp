#include "battery_icon.hpp"

#include <algorithm>

namespace opensu::ui {

float BatteryIcon::fillWidth(int percent, float inner, float sliver) noexcept {
    const int level = std::clamp(percent, 0, 100);
    if (level == 0) {
        return 0.0f;
    }
    return std::min(std::max(inner * static_cast<float>(level) / 100.0f, sliver), inner);
}

std::uint32_t BatteryIcon::boltColour(int percent) noexcept {
    if (percent >= 80) {
        return boltColours[0];
    }
    if (percent >= 60) {
        return boltColours[1];
    }
    return percent >= 30 ? boltColours[2] : boltColours[3];
}

} // namespace opensu::ui
