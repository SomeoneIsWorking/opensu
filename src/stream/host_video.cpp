#include "host_video.hpp"

#include <span>
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
    const std::optional<LinkPlan> plan = stream_.plan();
    if (!plan) {
        return;
    }
    bool key = stream_.takeKeyFrame();
    if (!encoder_ || plan->codec != codec_) {
        try {
            encoder_ = openEncoder(plan->codec);
        } catch (const std::runtime_error& failure) {
            lucent::warn("video", "cannot code with {}: {}", nameOf(plan->codec), failure.what());
            encoder_.reset();
            stream_.refuse(plan->codec);
            return;
        }
        codec_ = plan->codec;
        key = true;
    }
    CodedFrame coded;
    try {
        coded = encoder_->encode(frame, plan->bitsPerSecond, key);
    } catch (const std::runtime_error& failure) {
        stream_.end(failure.what());
        return;
    }
    if (coded.packets.empty()) {
        return;
    }
    const std::uint64_t encodedUs = wire::nowMicros();
    const std::uint32_t number = ++frames_;
    wire::VideoPacket packet{.frame = number,
                             .width = static_cast<std::uint16_t>(frame.width & ~1),
                             .height = static_cast<std::uint16_t>(frame.height & ~1),
                             .codec = static_cast<std::uint8_t>(codec_),
                             .key = coded.key,
                             .part = 0,
                             .parts = static_cast<std::uint16_t>(coded.packets.size()),
                             .capturedUs = frame.capturedUs,
                             .encodedUs = encodedUs,
                             .sentUs = wire::nowMicros(),
                             .data = {}};
    for (std::string& data : coded.packets) {
        packet.data = std::move(data);
        stream_.send(PacketKind::Video, wire::write(packet));
        ++packet.part;
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
    const std::span<const std::uint8_t> planes{last_};
    const std::size_t chromaAt = lastStride_ * static_cast<std::size_t>(lastHeight_);
    Picture picture{.width = lastWidth_, .height = lastHeight_, .rgba = {}};
    rgbaFromYcbcr(YcbcrPlanes{.luma = planes.subspan(0, chromaAt),
                              .blue = planes.subspan(chromaAt),
                              .red = planes.subspan(chromaAt + 1),
                              .width = lastWidth_,
                              .height = lastHeight_,
                              .lumaStride = lastStride_,
                              .chromaStride = lastStride_,
                              .chromaStep = 2},
                  picture.rgba);
    return std::pair{lastNumber_, std::move(picture)};
}

} // namespace opensu::stream
