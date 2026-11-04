#include "host_audio.hpp"

#include <stdexcept>
#include <utility>

#include "stream_wire.hpp"

namespace opensu::stream {

HostAudio::HostAudio(HostStream& stream, std::string sinkName)
    : stream_{stream},
      sound_{GameSound::Setup{.sinkName = std::move(sinkName),
                              .samples = [this](std::span<const float> samples,
                                                std::uint64_t capturedUs) {
                                  take(samples, capturedUs);
                              }}} {
}

void HostAudio::take(std::span<const float> samples, std::uint64_t capturedUs) {
    if (!stream_.joined() || stream_.over()) {
        pending_.clear();
        return;
    }
    pending_.insert(pending_.end(), samples.begin(), samples.end());
    constexpr std::size_t frameSamples = audioFrame * audioChannels;
    std::size_t used = 0;
    while (pending_.size() - used >= frameSamples) {
        std::string packet;
        try {
            packet = encoder_.encode(std::span<const float>{pending_}.subspan(used, frameSamples));
        } catch (const std::runtime_error& failure) {
            stream_.end(failure.what());
            return;
        }
        used += frameSamples;
        stream_.send(PacketKind::Audio, wire::write(wire::AudioPacket{.sequence = ++sequence_,
                                                                      .capturedUs = capturedUs,
                                                                      .data = std::move(packet)}));
    }
    pending_.erase(pending_.begin(), pending_.begin() + static_cast<std::ptrdiff_t>(used));
}

} // namespace opensu::stream
