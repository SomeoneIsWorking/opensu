#include "game_scaling.hpp"

namespace opensu::app {

bool scalingOnPlatform() noexcept {
    return false;
}

std::unique_ptr<ScalingOutput> openScalingOutput(bool /*insideGamescope*/) {
    return nullptr;
}

} // namespace opensu::app
