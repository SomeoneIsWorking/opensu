#include "brightness_control.hpp"

#include <algorithm>

namespace opensu::app {

std::optional<int> BrightnessControl::percent() const {
    return backlight_ != nullptr ? backlight_->percent() : std::nullopt;
}

std::string BrightnessControl::set(int percent) {
    // Fully dark leaves nothing to find the control by.
    return backlight_ != nullptr ? backlight_->setPercent(std::clamp(percent, 1, 100))
                                 : "no display brightness control";
}

std::string BrightnessControl::step(int delta) {
    const std::optional<int> now = percent();
    return now ? set(*now + delta) : "the brightness cannot be read";
}

} // namespace opensu::app
