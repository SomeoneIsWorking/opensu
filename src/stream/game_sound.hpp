// game_sound — the sound of a game the host streams, captured as 48 kHz stereo float samples. A
// game started for a guest plays into a PipeWire sink of its own (`environment`), not heard on the
// host, and that sink's output is captured; the sink goes with this. A game the host plays itself
// already plays on the host's output, so what that output plays is captured instead.
#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace opensu::stream {

class GameSound {
  public:
    struct Setup {
        /// The sink's node name, unique to the session; empty to capture the host's output.
        std::string sinkName;
        /// Interleaved samples as they come, with when they did, on the capture's thread.
        std::function<void(std::span<const float> samples, std::uint64_t capturedUs)> samples;
    };

    /// Makes the sink and listens to it; throws `std::runtime_error` saying why it cannot.
    explicit GameSound(Setup setup);
    ~GameSound();
    GameSound(const GameSound&) = delete;
    GameSound& operator=(const GameSound&) = delete;
    GameSound(GameSound&&) = delete;
    GameSound& operator=(GameSound&&) = delete;

    /// What the game's environment needs to play into the sink, through PipeWire or PulseAudio;
    /// nothing without one.
    [[nodiscard]] std::vector<std::string> environment() const;

  private:
    class Sink;
    std::string sinkName_;
    std::unique_ptr<Sink> sink_;
};

} // namespace opensu::stream
