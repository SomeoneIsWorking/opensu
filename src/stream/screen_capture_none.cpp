#include <stdexcept>

#include "screen_capture.hpp"

namespace opensu::stream {

class ScreenCapture::Stream {};

ScreenCapture::ScreenCapture(Setup /*setup*/) {
    throw std::runtime_error{"this system has no Gamescope stream to capture"};
}

ScreenCapture::~ScreenCapture() = default;

} // namespace opensu::stream
