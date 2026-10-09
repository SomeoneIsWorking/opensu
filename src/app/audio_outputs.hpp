// audio_outputs — the sound outputs the player can choose between, through the volume owner. The
// list is read when asked for, not on a timer, because reading it runs the mixer's own program.
#pragma once

#include <optional>
#include <string>
#include <vector>

#include "audio/system_volume.hpp"

namespace opensu::app {

class AudioOutputs {
  public:
    explicit AudioOutputs(audio::SystemVolume& volume) noexcept : volume_{volume} {
    }

    /// Reads the outputs again.
    void refresh();
    [[nodiscard]] const std::vector<audio::AudioSink>& sinks() const noexcept {
        return sinks_;
    }
    /// The default output, or nothing when none is known.
    [[nodiscard]] const audio::AudioSink* current() const;
    /// Makes output `id` the default; empty when done, else why not. Reads the list again.
    std::string choose(const std::string& id);
    /// Makes the output `step` places on from the default the default, wrapping; A is one.
    std::string cycle(int step);

  private:
    audio::SystemVolume& volume_;
    std::vector<audio::AudioSink> sinks_;
};

} // namespace opensu::app
