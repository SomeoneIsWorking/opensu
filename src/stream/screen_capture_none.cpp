#include <stdexcept>

#include "screen_capture.hpp"

namespace opensu::stream {

class ScreenCapture::Stream {};

std::string ScreenCapture::missing() {
    return "this system has no Gamescope stream to capture";
}

// NOLINTNEXTLINE(performance-unnecessary-value-param): the PipeWire backend moves it
ScreenCapture::ScreenCapture(Setup /*setup*/) {
    throw std::runtime_error{missing()};
}

ScreenCapture::~ScreenCapture() = default;

} // namespace opensu::stream
