#include "battery.hpp"

#include <algorithm>

namespace opensu::device {

std::optional<BatteryStatus> BatteryReader::read() const {
    int percent = -1;
    const SDL_PowerState state = SDL_GetPowerInfo(nullptr, &percent);
    return statusOf(state, percent);
}

std::optional<BatteryStatus> BatteryReader::statusOf(SDL_PowerState state, int percent) {
    const bool onBattery = state == SDL_POWERSTATE_ON_BATTERY || state == SDL_POWERSTATE_CHARGING ||
                           state == SDL_POWERSTATE_CHARGED;
    if (!onBattery || percent < 0) {
        return std::nullopt;
    }
    return BatteryStatus{std::clamp(percent, 0, 100), state == SDL_POWERSTATE_CHARGING};
}

} // namespace opensu::device
