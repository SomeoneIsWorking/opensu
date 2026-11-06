// live_sound — sound made as it plays (a remote game's), fed into a stream of its own on the
// shell's playback device. Packets come unevenly, so it starts, and starts again after running dry,
// only once `cushion` is held; past `deepest` queued it drops back so what plays stays near now.
#pragma once

#include <chrono>
#include <span>
#include <vector>

#include <SDL3/SDL_audio.h>

namespace opensu::audio {

class LiveSound {
  public:
    /// The sound held before playing starts.
    static constexpr std::chrono::milliseconds cushion{50};
    /// The most sound queued before the queue is dropped.
    static constexpr std::chrono::milliseconds deepest{150};

    /// Interleaved float samples at `rate` with `channels` played on `device`; throws
    /// `std::runtime_error` when SDL cannot route them.
    LiveSound(SDL_AudioDeviceID device, int rate, int channels);
    /// Logs how often it ran dry or dropped sound.
    ~LiveSound();
    LiveSound(const LiveSound&) = delete;
    LiveSound& operator=(const LiveSound&) = delete;
    LiveSound(LiveSound&&) = delete;
    LiveSound& operator=(LiveSound&&) = delete;

    /// Queues `samples` to play next. One thread at a time.
    void feed(std::span<const float> samples);

  private:
    SDL_AudioStream* stream_{nullptr};
    int cushionBytes_{0};
    int deepestBytes_{0};
    /// Sound waiting for the cushion to fill; it plays once it has.
    std::vector<float> held_;
    bool started_{false};
    int dry_{0};
    int dropped_{0};
};

} // namespace opensu::audio
