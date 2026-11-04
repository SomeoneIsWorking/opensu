// live_sound — sound made as it plays (a remote game's), fed from any thread into a stream of
// its own on the shell's playback device. It keeps at most `deepest` queued: past that the queue
// is dropped so what plays stays that close to now.
#pragma once

#include <chrono>
#include <span>

#include <SDL3/SDL_audio.h>

namespace opensu::audio {

class LiveSound {
  public:
    /// The most sound queued before the queue is dropped.
    static constexpr std::chrono::milliseconds deepest{60};

    /// Interleaved float samples at `rate` with `channels` played on `device`; throws
    /// `std::runtime_error` when SDL cannot route them.
    LiveSound(SDL_AudioDeviceID device, int rate, int channels);
    ~LiveSound();
    LiveSound(const LiveSound&) = delete;
    LiveSound& operator=(const LiveSound&) = delete;
    LiveSound(LiveSound&&) = delete;
    LiveSound& operator=(LiveSound&&) = delete;

    /// Queues `samples` to play next. Any thread.
    void feed(std::span<const float> samples);

  private:
    SDL_AudioStream* stream_{nullptr};
    int deepestBytes_{0};
};

} // namespace opensu::audio
