#include "live_sound.hpp"

#include <stdexcept>
#include <string>

#include <SDL3/SDL_error.h>

namespace opensu::audio {

LiveSound::LiveSound(SDL_AudioDeviceID device, int rate, int channels)
    : deepestBytes_{static_cast<int>(static_cast<long long>(rate) * channels *
                                      static_cast<long long>(sizeof(float)) * deepest.count() /
                                      1000)} {
    const SDL_AudioSpec spec{.format = SDL_AUDIO_F32, .channels = channels, .freq = rate};
    stream_ = SDL_CreateAudioStream(&spec, nullptr);
    if (stream_ == nullptr || !SDL_BindAudioStream(device, stream_)) {
        const std::string reason = SDL_GetError();
        SDL_DestroyAudioStream(stream_);
        throw std::runtime_error{"cannot play live sound: " + reason};
    }
}

LiveSound::~LiveSound() {
    SDL_DestroyAudioStream(stream_);
}

void LiveSound::feed(std::span<const float> samples) {
    if (SDL_GetAudioStreamQueued(stream_) > deepestBytes_) {
        SDL_ClearAudioStream(stream_);
    }
    SDL_PutAudioStreamData(stream_, samples.data(), static_cast<int>(samples.size_bytes()));
}

} // namespace opensu::audio
