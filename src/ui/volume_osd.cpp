#include "volume_osd.hpp"

namespace opensu::ui {

void VolumeOsd::show(const VolumeLevel& level, Clock::time_point now) noexcept {
    level_ = level;
    shownAt_ = now;
    if (!holding_) {
        fade_.show(now);
    }
    holding_ = true;
}

void VolumeOsd::tick(Clock::time_point now) noexcept {
    if (holding_ && now - shownAt_ >= volumeOsdHold) {
        fade_.hide(now);
        holding_ = false;
    }
}

} // namespace opensu::ui
