#include "remote_reach.hpp"

#include "audio_codec.hpp"
#include "screen_capture.hpp"
#include "udp_socket.hpp"
#include "video_codec.hpp"

namespace opensu::stream {

std::string hostingMissing() {
    return ScreenCapture::missing();
}

std::string playingMissing() {
    if (const std::string missing = udpMissing(); !missing.empty()) {
        return missing;
    }
    if (const std::string missing = audioCodecMissing(); !missing.empty()) {
        return missing;
    }
    return decoderMissing();
}

} // namespace opensu::stream
