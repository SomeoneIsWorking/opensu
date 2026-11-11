// sound_loop — one clip played over and over on the shell's playback device in a stream of its
// own (a theme's menu music), fed from SDL's audio thread as the stream runs low, so the frame
// loop never has to keep it filled. Stopping lets the stream run dry; playing again goes on from
// where it stopped.
#pragma once

#include <atomic>
#include <cstddef>

#include <SDL3/SDL_audio.h>

#include "sound_clip.hpp"

namespace opensu::audio {

class SoundLoop {
  public:
    /// `clip`, which holds sound, looped on `device`; throws `std::runtime_error` when SDL cannot
    /// route it. Starts stopped.
    SoundLoop(SDL_AudioDeviceID device, SoundClip clip);
    ~SoundLoop();
    SoundLoop(const SoundLoop&) = delete;
    SoundLoop& operator=(const SoundLoop&) = delete;
    SoundLoop(SoundLoop&&) = delete;
    SoundLoop& operator=(SoundLoop&&) = delete;

    /// Plays or stops it.
    void play(bool on) noexcept;
    [[nodiscard]] bool playing() const noexcept {
        return playing_.load(std::memory_order_relaxed);
    }

  private:
    /// SDL's audio thread asking for `wanted` more bytes.
    static void SDLCALL more(void* loop, SDL_AudioStream* stream, int wanted, int total);
    void feed(SDL_AudioStream* stream, int wanted);

    SoundClip clip_;
    SDL_AudioStream* stream_{nullptr};
    std::atomic<bool> playing_{false};
    /// The next byte of the clip to play; the audio thread's alone.
    std::size_t at_{0};
};

} // namespace opensu::audio
