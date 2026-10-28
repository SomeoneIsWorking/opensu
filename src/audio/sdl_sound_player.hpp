// sdl_sound_player — plays the UI sounds on the default SDL audio device, one audio stream per
// effect so they overlap. The files are openSU's own set; an effect without a file, or a machine
// without an audio device, plays nothing.
#pragma once

#include <array>
#include <cstddef>
#include <filesystem>

#include <SDL3/SDL_audio.h>

#include "debounce.hpp"
#include "effect.hpp"
#include "sound_clip.hpp"

namespace opensu::audio {

class SdlSoundPlayer {
  public:
    SdlSoundPlayer() = default;
    ~SdlSoundPlayer();
    SdlSoundPlayer(const SdlSoundPlayer&) = delete;
    SdlSoundPlayer& operator=(const SdlSoundPlayer&) = delete;
    SdlSoundPlayer(SdlSoundPlayer&&) = delete;
    SdlSoundPlayer& operator=(SdlSoundPlayer&&) = delete;

    /// Opens the default playback device once; without one it logs a warning and stays silent.
    /// A hidden shell and the tests never call it.
    void open();

    /// Whether the audio device is open.
    [[nodiscard]] bool ready() const noexcept {
        return device_ != 0;
    }

    /// Decodes an effect's WAV or OGG, replacing the one loaded; works without a device. False
    /// when the file is unusable.
    bool load(Effect effect, const std::filesystem::path& file);

    /// Decodes every effect's file in `directory` (`shippedFile`); a missing or unusable one is
    /// logged and stays silent. How many loaded.
    std::size_t loadSet(const std::filesystem::path& directory);

    /// Whether the effect has a decoded clip.
    [[nodiscard]] bool loaded(Effect effect) const noexcept;

    /// Silences every effect, or lets them play again.
    void setMuted(bool muted) noexcept {
        muted_ = muted;
    }
    [[nodiscard]] bool muted() const noexcept {
        return muted_;
    }

    /// Plays the effect from its start, unless muted, silent, without a file or dropped by its
    /// debounce. Whether it started.
    bool play(Effect effect);

  private:
    /// Binds a stream for each loaded effect that has none.
    void bindStreams();

    SDL_AudioDeviceID device_{0};
    bool audioUp_{false};
    bool muted_{false};
    /// Empty PCM for an effect with no file.
    std::array<SoundClip, allEffects.size()> clips_{};
    std::array<SDL_AudioStream*, allEffects.size()> streams_{};
    Debounce debounce_;
};

} // namespace opensu::audio
