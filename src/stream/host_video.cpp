#include "host_video.hpp"

#include <algorithm>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>

#include "lucent/log.h"
#include "stream_wire.hpp"

namespace opensu::stream {

HostVideo::HostVideo(Audience& audience, const std::filesystem::path& gamescopeSocket)
    : audience_{audience},
      capture_{ScreenCapture::Setup{.gamescopeSocket = gamescopeSocket,
                                    .frame = [this](const CapturedFrame& frame) {
                                        send(frame);
                                    }}} {
}

void HostVideo::send(const CapturedFrame& frame) {
    audience_.visit([this, &frame](std::span<HostStream* const> streams) {
        std::map<VideoCodec, Group> groups;
        for (HostStream* stream : streams) {
            if (const std::optional<LinkPlan> plan = stream->plan()) {
                Group& group = groups[plan->codec];
                group.bitsPerSecond = group.guests.empty()
                                          ? plan->bitsPerSecond
                                          : std::min(group.bitsPerSecond, plan->bitsPerSecond);
                group.guests.push_back(stream);
            }
        }
        std::erase_if(encodings_, [&groups](const auto& encoding) {
            return !groups.contains(encoding.first);
        });
        if (groups.empty()) {
            return;
        }
        const std::uint32_t number = frames_ + 1;
        bool sent = false;
        for (const auto& [codec, group] : groups) {
            sent = code(frame, codec, group, number) || sent;
        }
        if (!sent) {
            return;
        }
        frames_ = number;
        const std::lock_guard lock{lastMutex_};
        lastNumber_ = number;
        lastWidth_ = frame.width & ~1;
        lastHeight_ = frame.height & ~1;
        lastStride_ = frame.stride;
        last_.assign(frame.luma.begin(), frame.luma.end());
        last_.insert(last_.end(), frame.chroma.begin(), frame.chroma.end());
    });
}

bool HostVideo::code(const CapturedFrame& frame, VideoCodec codec, const Group& group,
                     std::uint32_t number) {
    bool key = false;
    for (HostStream* stream : group.guests) {
        key = stream->takeKeyFrame() || key;
    }
    Encoding& encoding = encodings_[codec];
    if (!encoding.encoder) {
        try {
            encoding.encoder = openEncoder(codec);
        } catch (const std::runtime_error& failure) {
            lucent::warn("video", "cannot code with {}: {}", nameOf(codec), failure.what());
            encodings_.erase(codec);
            for (HostStream* stream : group.guests) {
                stream->refuse(codec);
            }
            return false;
        }
        key = true;
    }
    const std::vector<const HostStream*> guests{group.guests.begin(), group.guests.end()};
    if (guests != encoding.guests || group.bitsPerSecond != encoding.bitsPerSecond) {
        // A guest new to this codec starts on a key frame.
        key = key || std::ranges::any_of(guests, [&encoding](const HostStream* stream) {
                  return std::ranges::find(encoding.guests, stream) == encoding.guests.end();
              });
        if (guests != encoding.guests) {
            lucent::info("video", "coding {} at {:.1f} Mbit/s for {} guest{}", nameOf(codec),
                         static_cast<double>(group.bitsPerSecond) / 1e6, guests.size(),
                         guests.size() == 1 ? "" : "s");
        }
        encoding.guests = guests;
        encoding.bitsPerSecond = group.bitsPerSecond;
    }
    CodedFrame coded;
    try {
        coded = encoding.encoder->encode(frame, group.bitsPerSecond, key);
    } catch (const std::runtime_error& failure) {
        encodings_.erase(codec);
        for (HostStream* stream : group.guests) {
            stream->end(failure.what());
        }
        return false;
    }
    if (coded.packets.empty()) {
        return false;
    }
    const std::uint64_t encodedUs = wire::nowMicros();
    wire::VideoPacket packet{.frame = number,
                             .width = static_cast<std::uint16_t>(frame.width & ~1),
                             .height = static_cast<std::uint16_t>(frame.height & ~1),
                             .codec = static_cast<std::uint8_t>(codec),
                             .key = coded.key,
                             .part = 0,
                             .parts = static_cast<std::uint16_t>(coded.packets.size()),
                             .capturedUs = frame.capturedUs,
                             .encodedUs = encodedUs,
                             .sentUs = wire::nowMicros(),
                             .data = {}};
    std::vector<std::string> payloads;
    payloads.reserve(coded.packets.size());
    for (std::string& data : coded.packets) {
        packet.data = std::move(data);
        payloads.push_back(wire::write(packet));
        ++packet.part;
    }
    for (std::size_t i = 0; i + 1 < group.guests.size(); ++i) {
        group.guests[i]->sendFrame(payloads);
    }
    group.guests.back()->sendFrame(std::move(payloads));
    return true;
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
