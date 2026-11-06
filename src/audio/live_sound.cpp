#include "live_sound.hpp"

#include <stdexcept>
#include <string>

#include <SDL3/SDL_error.h>

#include "lucent/log.h"

namespace opensu::audio {
namespace {

int bytesOf(std::chrono::milliseconds length, int rate, int channels) {
    return static_cast<int>(static_cast<long long>(rate) * channels *
                            static_cast<long long>(sizeof(float)) * length.count() / 1000);
}

} // namespace

LiveSound::LiveSound(SDL_AudioDeviceID device, int rate, int channels)
    : cushionBytes_{bytesOf(cushion, rate, channels)},
      deepestBytes_{bytesOf(deepest, rate, channels)} {
    const SDL_AudioSpec spec{.format = SDL_AUDIO_F32, .channels = channels, .freq = rate};
    stream_ = SDL_CreateAudioStream(&spec, nullptr);
    if (stream_ == nullptr || !SDL_BindAudioStream(device, stream_)) {
        const std::string reason = SDL_GetError();
        SDL_DestroyAudioStream(stream_);
        throw std::runtime_error{"cannot play live sound: " + reason};
    }
}

LiveSound::~LiveSound() {
    lucent::info("sound", "live sound ran dry {} times and dropped {} times", dry_, dropped_);
    SDL_DestroyAudioStream(stream_);
}

void LiveSound::feed(std::span<const float> samples) {
    const int queued = SDL_GetAudioStreamQueued(stream_);
    if (queued > deepestBytes_) {
        SDL_ClearAudioStream(stream_);
        ++dropped_;
        started_ = false;
    } else if (started_ && queued == 0) {
        ++dry_;
        started_ = false;
    }
    if (started_) {
        SDL_PutAudioStreamData(stream_, samples.data(), static_cast<int>(samples.size_bytes()));
        return;
    }
    held_.insert(held_.end(), samples.begin(), samples.end());
    if (std::span<const float>{held_}.size_bytes() >= static_cast<std::size_t>(cushionBytes_)) {
        SDL_PutAudioStreamData(stream_, held_.data(),
                               static_cast<int>(std::span<const float>{held_}.size_bytes()));
        held_.clear();
        started_ = true;
    }
}

} // namespace opensu::audio
