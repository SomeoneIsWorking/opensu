#include "frame_generation.hpp"

namespace opensu::app {

bool frameGenOnPlatform() noexcept {
    return false;
}

std::unique_ptr<FrameGenOutput> openFrameGenOutput(bool /*insideGamescope*/) {
    return nullptr;
}

} // namespace opensu::app
