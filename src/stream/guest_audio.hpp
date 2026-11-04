// guest_audio — the guest's sound for one session: each Opus packet decoded as it comes, a lost one
// filled in by Opus, and the samples handed to `play` (nothing plays when it is empty). Counts what
// came and what was lost, and how long sound took from the host's capture to here.
#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <span>
#include <string>
#include <vector>

#include "audio_codec.hpp"
#include "stream_wire.hpp"

namespace opensu::stream {

class GuestAudio {
  public:
    /// Throws `std::runtime_error` when Opus refuses.
    explicit GuestAudio(std::function<void(std::span<const float>)> play);

    /// One packet, which came at `receivedUs` while the host's clock was `hostAheadUs` ahead.
    /// On the stream's thread.
    void take(const wire::AudioPacket& packet, std::uint64_t receivedUs, std::int64_t hostAheadUs);

    /// What came, what was lost, and the 50th and 95th percentile of capture to arrival in
    /// milliseconds, as one line. Any thread.
    [[nodiscard]] std::string summary() const;

  private:
    std::function<void(std::span<const float>)> play_;
    AudioDecoder decoder_;
    std::vector<float> samples_;
    std::uint32_t last_{0};

    mutable std::mutex mutex_;
    std::size_t packets_{0};
    std::size_t lost_{0};
    std::deque<double> arrivals_;
};

} // namespace opensu::stream
