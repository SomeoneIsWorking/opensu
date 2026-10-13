// audio_sink — the core's stereo samples resampled into a ring the sound device drains. The
// resampling ratio stays within ±0.5 % of nominal, nudged by the ring's fill toward its target, so
// a core running on a display clock neither under- nor over-runs the device.
#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <span>
#include <vector>

namespace opensu::retro {

class AudioSink {
  public:
    /// The furthest the ratio moves from nominal.
    static constexpr double maxAdjustment = 0.005;

    /// Frames go out at `outputRate`; the ring aims to hold `targetFrames` of them.
    AudioSink(int outputRate, std::size_t targetFrames);

    /// The core's sample rate; nothing is resampled until it is set.
    void setCoreRate(double rate);

    /// Interleaved stereo frames from the core, main thread.
    void push(std::span<const std::int16_t> interleaved);

    /// Fills `out` with interleaved stereo frames, silence where the ring runs dry; any thread.
    void pull(std::span<std::int16_t> out);
    /// Drops `frames` from the ring as if played, for running without a device.
    void discard(std::size_t frames);

    [[nodiscard]] int outputRate() const {
        return outputRate_;
    }
    [[nodiscard]] std::size_t targetFrames() const {
        return targetFrames_;
    }
    /// Output frames per core frame at the present fill.
    [[nodiscard]] double ratio() const;
    [[nodiscard]] double nominalRatio() const;
    [[nodiscard]] std::size_t fill() const;
    /// Output frames that were played as silence because the ring was empty.
    [[nodiscard]] std::size_t underrunFrames() const;
    /// Output frames dropped because the ring was full.
    [[nodiscard]] std::size_t overrunFrames() const;

  private:
    void store(std::int16_t left, std::int16_t right);
    [[nodiscard]] double ratioAt(std::size_t fill) const;

    const int outputRate_;
    const std::size_t targetFrames_;
    double coreRate_{0.0};

    // Main thread only: the resampler's position between the previous and next core frame.
    double phase_{0.0};
    std::int16_t lastLeft_{0};
    std::int16_t lastRight_{0};

    mutable std::mutex mutex_;
    std::vector<std::int16_t> ring_;
    std::size_t read_{0};
    std::size_t count_{0};
    std::size_t underruns_{0};
    std::size_t overruns_{0};
};

} // namespace opensu::retro
