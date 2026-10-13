#include "audio_sink.hpp"

#include <algorithm>
#include <cmath>

namespace opensu::retro {
namespace {

constexpr std::size_t channels = 2;

} // namespace

AudioSink::AudioSink(int outputRate, std::size_t targetFrames)
    : outputRate_{outputRate}, targetFrames_{std::max<std::size_t>(targetFrames, 1)},
      ring_(targetFrames_ * 4 * channels) {
}

void AudioSink::setCoreRate(double rate) {
    coreRate_ = rate;
}

double AudioSink::nominalRatio() const {
    return coreRate_ > 0.0 ? outputRate_ / coreRate_ : 0.0;
}

double AudioSink::ratioAt(std::size_t fill) const {
    const double target = static_cast<double>(targetFrames_);
    const double error = std::clamp((target - static_cast<double>(fill)) / target, -1.0, 1.0);
    return nominalRatio() * (1.0 + maxAdjustment * error);
}

double AudioSink::ratio() const {
    return ratioAt(fill());
}

std::size_t AudioSink::fill() const {
    const std::lock_guard lock{mutex_};
    return count_;
}

std::size_t AudioSink::underrunFrames() const {
    const std::lock_guard lock{mutex_};
    return underruns_;
}

std::size_t AudioSink::overrunFrames() const {
    const std::lock_guard lock{mutex_};
    return overruns_;
}

void AudioSink::push(std::span<const std::int16_t> interleaved) {
    if (coreRate_ <= 0.0) {
        return;
    }
    // Core frames advanced per output frame.
    const double step = 1.0 / ratio();
    const std::lock_guard lock{mutex_};
    for (std::size_t at = 0; at + 1 < interleaved.size(); at += channels) {
        const std::int16_t left = interleaved[at];
        const std::int16_t right = interleaved[at + 1];
        while (phase_ < 1.0) {
            const auto mix = [this](std::int16_t from, std::int16_t to) {
                return static_cast<std::int16_t>(std::lround(from + (to - from) * phase_));
            };
            store(mix(lastLeft_, left), mix(lastRight_, right));
            phase_ += step;
        }
        phase_ -= 1.0;
        lastLeft_ = left;
        lastRight_ = right;
    }
}

void AudioSink::store(std::int16_t left, std::int16_t right) {
    const std::size_t capacity = ring_.size() / channels;
    if (count_ == capacity) {
        ++overruns_;
        return;
    }
    const std::size_t slot = (read_ + count_) % capacity;
    ring_[slot * channels] = left;
    ring_[slot * channels + 1] = right;
    ++count_;
}

void AudioSink::pull(std::span<std::int16_t> out) {
    const std::size_t capacity = ring_.size() / channels;
    const std::size_t wanted = out.size() / channels;
    const std::lock_guard lock{mutex_};
    const std::size_t taken = std::min(wanted, count_);
    for (std::size_t frame = 0; frame < taken; ++frame) {
        const std::size_t slot = (read_ + frame) % capacity;
        out[frame * channels] = ring_[slot * channels];
        out[frame * channels + 1] = ring_[slot * channels + 1];
    }
    std::fill(out.begin() + static_cast<std::ptrdiff_t>(taken * channels), out.end(), 0);
    read_ = (read_ + taken) % capacity;
    count_ -= taken;
    underruns_ += wanted - taken;
}

void AudioSink::discard(std::size_t frames) {
    const std::size_t capacity = ring_.size() / channels;
    const std::lock_guard lock{mutex_};
    const std::size_t taken = std::min(frames, count_);
    read_ = (read_ + taken) % capacity;
    count_ -= taken;
    underruns_ += frames - taken;
}

} // namespace opensu::retro
