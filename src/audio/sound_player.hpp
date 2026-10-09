// sound_player — plays iiSU's UI sounds through raylib's audio device. The files come from the
// artwork store; an effect without a file, or a machine without an audio device, plays nothing.
#pragma once

#include <array>
#include <filesystem>

#include "debounce.hpp"
#include "effect.hpp"
#include "raylib.h"

namespace opensu::audio {

class SoundPlayer {
  public:
    SoundPlayer() = default;
    ~SoundPlayer();
    SoundPlayer(const SoundPlayer&) = delete;
    SoundPlayer& operator=(const SoundPlayer&) = delete;

    /// Opens the audio device once. Without one it logs a warning and the player stays silent.
    void open();

    /// Whether the audio device is open.
    [[nodiscard]] bool ready() const noexcept {
        return ready_;
    }

    /// Loads an effect's WAV, replacing the one loaded. False when silent or the file is unusable.
    bool load(Effect effect, const std::filesystem::path& file);

    /// Silences every effect, or lets them play again.
    void setMuted(bool muted) noexcept {
        muted_ = muted;
    }
    [[nodiscard]] bool muted() const noexcept {
        return muted_;
    }

    /// Plays the effect now, unless its debounce drops it or it has no file. Whether it started.
    bool play(Effect effect);

  private:
    bool ready_{false};
    bool muted_{false};
    std::array<Sound, allEffects.size()> sounds_{};
    std::array<bool, allEffects.size()> loaded_{};
    Debounce debounce_;
};

} // namespace opensu::audio
