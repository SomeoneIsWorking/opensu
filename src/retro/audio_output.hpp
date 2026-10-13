// audio_output — where the sink's frames go: the default sound device through SDL, or, for a hidden
// run, nowhere at the device's pace, so the sink's fill and rate control behave the same.
#pragma once

#include <chrono>
#include <cstdint>
#include <vector>

#include <SDL3/SDL_audio.h>

#include "audio_sink.hpp"

namespace opensu::retro {

class AudioOutput {
  public:
    using Clock = std::chrono::steady_clock;

    AudioOutput() = default;
    AudioOutput(const AudioOutput&) = delete;
    AudioOutput& operator=(const AudioOutput&) = delete;
    AudioOutput(AudioOutput&&) = delete;
    AudioOutput& operator=(AudioOutput&&) = delete;
    virtual ~AudioOutput() = default;

    /// Called once per loop iteration on the main thread.
    virtual void service(Clock::time_point now) = 0;
};

/// The sink played on the default playback device.
class DeviceOutput final : public AudioOutput {
  public:
    /// Throws std::runtime_error when no device opens.
    explicit DeviceOutput(AudioSink& sink);
    ~DeviceOutput() override;
    DeviceOutput(const DeviceOutput&) = delete;
    DeviceOutput& operator=(const DeviceOutput&) = delete;
    DeviceOutput(DeviceOutput&&) = delete;
    DeviceOutput& operator=(DeviceOutput&&) = delete;

    void service(Clock::time_point now) override;

  private:
    /// The device thread asking for `bytes` more.
    void feed(SDL_AudioStream* stream, int bytes);

    AudioSink& sink_;
    SDL_AudioStream* stream_{nullptr};
    /// The device thread's buffer.
    std::vector<std::int16_t> buffer_;
};

/// The sink drained at its output rate by the clock; nothing is played.
class ClockOutput final : public AudioOutput {
  public:
    explicit ClockOutput(AudioSink& sink);

    void service(Clock::time_point now) override;

  private:
    AudioSink& sink_;
    Clock::time_point last_{};
    double owed_{0.0};
};

} // namespace opensu::retro
