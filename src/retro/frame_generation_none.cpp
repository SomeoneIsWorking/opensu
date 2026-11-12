#include "frame_generation.hpp"

namespace opensu::retro {

std::unique_ptr<FrameGeneration>
FrameGeneration::open(const settings::FrameGenSettings& /*settings*/, std::string& why) {
    why = "frame generation is not built here";
    return nullptr;
}

} // namespace opensu::retro
