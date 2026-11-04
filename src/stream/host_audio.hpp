// host_audio — the host's sound for one session: the game's own sink captured, cut into 10 ms
// frames, each coded with Opus and sent to the guest in a datagram of its own with when it was
// captured. Sound before the guest joined is not sent.
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "audio_codec.hpp"
#include "game_sound.hpp"
#include "host_stream.hpp"

namespace opensu::stream {

class HostAudio {
  public:
    /// Sends the sound of a sink named `sinkName` over `stream`, which outlives this. Throws
    /// `std::runtime_error` when the sound cannot be captured or coded.
    HostAudio(HostStream& stream, std::string sinkName);

    /// What the game's environment needs for its sound to be sent.
    [[nodiscard]] std::vector<std::string> environment() const {
        return sound_.environment();
    }

  private:
    void take(std::span<const float> samples, std::uint64_t capturedUs);

    HostStream& stream_;
    AudioEncoder encoder_;
    std::vector<float> pending_;
    std::uint32_t sequence_{0};
    /// Last, so it stops before what it uses goes.
    GameSound sound_;
};

} // namespace opensu::stream
