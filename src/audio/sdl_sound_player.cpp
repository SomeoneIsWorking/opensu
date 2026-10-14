#include "sdl_sound_player.hpp"

#include <utility>

#include <SDL3/SDL_init.h>

#include "lucent/log.h"

namespace opensu::audio {
namespace {

std::size_t slot(Effect effect) {
    return static_cast<std::size_t>(effect);
}

} // namespace

SdlSoundPlayer::~SdlSoundPlayer() {
    for (SDL_AudioStream* stream : streams_) {
        if (stream != nullptr) {
            SDL_DestroyAudioStream(stream);
        }
    }
    if (device_ != 0) {
        SDL_CloseAudioDevice(device_);
    }
    if (audioUp_) {
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
    }
}

void SdlSoundPlayer::open() {
    if (device_ != 0) {
        return;
    }
    if (!audioUp_) {
        audioUp_ = SDL_InitSubSystem(SDL_INIT_AUDIO);
    }
    if (audioUp_) {
        device_ = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, nullptr);
    }
    if (device_ == 0) {
        lucent::warn("audio", "no audio device ({}); UI sounds are silent", SDL_GetError());
        return;
    }
    bindStreams();
}

bool SdlSoundPlayer::load(Effect effect, const std::filesystem::path& file) {
    std::optional<SoundClip> clip = decodeSound(file);
    if (!clip) {
        return false;
    }
    SDL_AudioStream*& stream = streams_[slot(effect)];
    if (stream != nullptr) {
        SDL_DestroyAudioStream(stream);
        stream = nullptr;
    }
    clips_[slot(effect)] = std::move(*clip);
    bindStreams();
    return true;
}

bool SdlSoundPlayer::loaded(Effect effect) const noexcept {
    return !clips_[slot(effect)].pcm.empty();
}

void SdlSoundPlayer::bindStreams() {
    if (device_ == 0) {
        return;
    }
    for (std::size_t i = 0; i < clips_.size(); ++i) {
        if (clips_[i].pcm.empty() || streams_[i] != nullptr) {
            continue;
        }
        // A null destination spec takes the device's format once bound.
        SDL_AudioStream* stream = SDL_CreateAudioStream(&clips_[i].spec, nullptr);
        if (stream == nullptr || !SDL_BindAudioStream(device_, stream)) {
            lucent::warn("audio", "cannot route a UI sound to the device: {}", SDL_GetError());
            SDL_DestroyAudioStream(stream);
            continue;
        }
        streams_[i] = stream;
    }
}

bool SdlSoundPlayer::play(Effect effect) {
    SDL_AudioStream* stream = streams_[slot(effect)];
    if (muted_ || stream == nullptr || !debounce_.admit(effect, Debounce::Clock::now())) {
        return false;
    }
    const SoundClip& clip = clips_[slot(effect)];
    // A replay restarts the sound.
    SDL_ClearAudioStream(stream);
    return SDL_PutAudioStreamData(stream, clip.pcm.data(), static_cast<int>(clip.pcm.size()));
}

} // namespace opensu::audio
