// host_audio — the host's sound for one session: the game's own sink, or the host's output,
// captured once, cut into 10 ms frames, each coded with Opus once and sent to every guest of the
// audience in a datagram of its own with when it was captured. Sound while no guest has joined is
// not sent.
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "audience.hpp"
#include "audio_codec.hpp"
#include "game_sound.hpp"

namespace opensu::stream {

class HostAudio {
  public:
    /// Sends the sound of a sink of its own named `sinkName`, or with an empty name what the
    /// host's output plays, to `audience`, which outlives this. Throws `std::runtime_error` when
    /// the sound cannot be captured or coded.
    HostAudio(Audience& audience, std::string sinkName);

    /// What the game's environment needs for its sound to be sent.
    [[nodiscard]] std::vector<std::string> environment() const {
        return sound_.environment();
    }

  private:
    void take(std::span<const float> samples, std::uint64_t capturedUs);

    Audience& audience_;
    AudioEncoder encoder_;
    std::vector<float> pending_;
    std::uint32_t sequence_{0};
    /// Last, so it stops before what it uses goes.
    GameSound sound_;
};

} // namespace opensu::stream
