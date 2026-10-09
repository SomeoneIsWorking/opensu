#include "battery_icon.hpp"

#include <algorithm>

namespace opensu::ui {

int BatteryIcon::slot(int percent, bool charging) noexcept {
    const int level = std::clamp(percent, 0, 100);
    if (charging) {
        if (level == 100) {
            return 0;
        }
        if (level >= 80) {
            return 1;
        }
        if (level >= 60) {
            return 2;
        }
        return level < 30 ? 4 : 3;
    }
    if (level >= 80) {
        return 0;
    }
    if (level >= 60) {
        return 1;
    }
    if (level >= 40) {
        return 2;
    }
    return level < 20 ? 4 : 3;
}

} // namespace opensu::ui
