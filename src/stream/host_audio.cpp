#include "host_audio.hpp"

#include <stdexcept>
#include <utility>

#include "stream_wire.hpp"

namespace opensu::stream {

HostAudio::HostAudio(Audience& audience, std::string sinkName)
    : audience_{audience}, sound_{GameSound::Setup{.sinkName = std::move(sinkName),
                                                   .samples = [this](std::span<const float> samples,
                                                                     std::uint64_t capturedUs) {
                                                       take(samples, capturedUs);
                                                   }}} {
}

void HostAudio::take(std::span<const float> samples, std::uint64_t capturedUs) {
    audience_.visit([&](std::span<HostStream* const> streams) {
        if (streams.empty()) {
            pending_.clear();
            return;
        }
        pending_.insert(pending_.end(), samples.begin(), samples.end());
        constexpr std::size_t frameSamples = audioFrame * audioChannels;
        std::size_t used = 0;
        while (pending_.size() - used >= frameSamples) {
            std::string packet;
            try {
                packet =
                    encoder_.encode(std::span<const float>{pending_}.subspan(used, frameSamples));
            } catch (const std::runtime_error& failure) {
                for (HostStream* stream : streams) {
                    stream->end(failure.what());
                }
                pending_.clear();
                return;
            }
            used += frameSamples;
            const std::string payload = wire::write(wire::AudioPacket{
                .sequence = ++sequence_, .capturedUs = capturedUs, .data = std::move(packet)});
            for (HostStream* stream : streams) {
                stream->send(PacketKind::Audio, payload);
            }
        }
        pending_.erase(pending_.begin(), pending_.begin() + static_cast<std::ptrdiff_t>(used));
    });
}

} // namespace opensu::stream
