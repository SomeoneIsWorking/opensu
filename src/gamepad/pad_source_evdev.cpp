#include "evdev_pads.hpp"
#include "pad_source.hpp"

namespace opensu::gamepad {

std::unique_ptr<PadSource> openPadSource(const PadSourceOptions& options) {
    if (options.hiddenInputs) {
        std::filesystem::create_directories(*options.hiddenInputs);
        return std::make_unique<EvdevPads>(*options.hiddenInputs, options.own);
    }
    return std::make_unique<EvdevPads>("/dev/input", options.own);
}

} // namespace opensu::gamepad
