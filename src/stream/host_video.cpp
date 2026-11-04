#include "host_video.hpp"

#include <algorithm>
#include <stdexcept>

#include "lucent/log.h"
#include "stream_wire.hpp"

namespace opensu::stream {

HostVideo::HostVideo(HostStream& stream, const std::filesystem::path& gamescopeSocket)
    : stream_{stream}, capture_{ScreenCapture::Setup{.gamescopeSocket = gamescopeSocket,
                                                     .frame = [this](const CapturedFrame& frame) {
                                                         send(frame);
                                                     }}} {
}

void HostVideo::send(const CapturedFrame& frame) {
    if (!stream_.joined() || stream_.over()) {
        return;
    }
    std::vector<std::string> packets;
    try {
        packets = encoder_.encode(frame, frameBudget);
    } catch (const std::runtime_error& failure) {
        stream_.end(failure.what());
        return;
    }
    const std::uint64_t encodedUs = wire::nowMicros();
    const std::uint32_t number = ++frames_;
    wire::VideoPacket packet{.frame = number,
                             .width = static_cast<std::uint16_t>(frame.width & ~1),
                             .height = static_cast<std::uint16_t>(frame.height & ~1),
                             .capturedUs = frame.capturedUs,
                             .encodedUs = encodedUs,
                             .sentUs = wire::nowMicros(),
                             .data = {}};
    for (std::string& data : packets) {
        packet.data = std::move(data);
        stream_.send(PacketKind::Video, wire::write(packet));
    }
    const std::lock_guard lock{lastMutex_};
    lastNumber_ = number;
    lastWidth_ = packet.width;
    lastHeight_ = packet.height;
    lastStride_ = frame.stride;
    last_.assign(frame.luma.begin(), frame.luma.end());
    last_.insert(last_.end(), frame.chroma.begin(), frame.chroma.end());
}

std::optional<std::pair<std::uint32_t, Picture>> HostVideo::lastSent() const {
    const std::lock_guard lock{lastMutex_};
    if (lastNumber_ == 0) {
        return std::nullopt;
    }
    // The interleaved chroma split into its planes, as the decoder gives them.
    const auto width = static_cast<std::size_t>(lastWidth_);
    const auto height = static_cast<std::size_t>(lastHeight_);
    std::vector<std::uint8_t> luma(width * height);
    std::vector<std::uint8_t> blue(width * height / 4);
    std::vector<std::uint8_t> red(width * height / 4);
    const std::size_t chromaAt = lastStride_ * height;
    for (std::size_t y = 0; y < height; ++y) {
        std::copy_n(last_.begin() + static_cast<std::ptrdiff_t>(y * lastStride_), width,
                    luma.begin() + static_cast<std::ptrdiff_t>(y * width));
    }
    for (std::size_t y = 0; y < height / 2; ++y) {
        for (std::size_t x = 0; x < width / 2; ++x) {
            const std::size_t from = chromaAt + y * lastStride_ + 2 * x;
            blue[y * (width / 2) + x] = last_[from];
            red[y * (width / 2) + x] = last_[from + 1];
        }
    }
    Picture picture{.width = lastWidth_, .height = lastHeight_, .rgba = {}};
    rgbaFromYcbcr(
        YcbcrPlanes{
            .luma = luma, .blue = blue, .red = red, .width = lastWidth_, .height = lastHeight_},
        picture.rgba);
    return std::pair{lastNumber_, std::move(picture)};
}

} // namespace opensu::stream
