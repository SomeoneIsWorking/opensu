#include "remote_reach.hpp"

#include "screen_capture.hpp"
#include "video_codec.hpp"

namespace opensu::stream {

std::string hostingMissing() {
    return ScreenCapture::missing();
}

std::string playingMissing() {
    return decoderMissing();
}

} // namespace opensu::stream
