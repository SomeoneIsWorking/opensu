#include "pad_source.hpp"
#include "sdl_pads.hpp"

namespace opensu::gamepad {

std::unique_ptr<PadSource> openPadSource(const PadSourceOptions& options) {
    return std::make_unique<SdlPads>(!options.hiddenInputs.has_value());
}

} // namespace opensu::gamepad
