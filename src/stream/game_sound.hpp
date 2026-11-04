// game_sound — the sound of a game started for a guest: a PipeWire sink of its own that the game
// is told to play into (`environment`), and that sink's output, captured as 48 kHz stereo float
// samples. The game is not heard on the host. The sink goes with this.
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
        /// The sink's node name, unique to the session.
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

    /// What the game's environment needs to play into the sink, through PipeWire or PulseAudio.
    [[nodiscard]] std::vector<std::string> environment() const;

  private:
    class Sink;
    std::string sinkName_;
    std::unique_ptr<Sink> sink_;
};

} // namespace opensu::stream
