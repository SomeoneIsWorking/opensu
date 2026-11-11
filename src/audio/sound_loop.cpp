#include "sound_loop.hpp"

#include <algorithm>
#include <stdexcept>
#include <string>
#include <utility>

#include <SDL3/SDL_error.h>

namespace opensu::audio {

SoundLoop::SoundLoop(SDL_AudioDeviceID device, SoundClip clip) : clip_{std::move(clip)} {
    if (clip_.pcm.empty()) {
        throw std::runtime_error{"cannot loop a sound that holds none"};
    }
    stream_ = SDL_CreateAudioStream(&clip_.spec, nullptr);
    if (stream_ == nullptr || !SDL_SetAudioStreamGetCallback(stream_, &SoundLoop::more, this) ||
        !SDL_BindAudioStream(device, stream_)) {
        const std::string reason = SDL_GetError();
        SDL_DestroyAudioStream(stream_);
        throw std::runtime_error{"cannot loop a sound on the device: " + reason};
    }
}

SoundLoop::~SoundLoop() {
    // Holds the stream's lock, so no callback runs past it.
    SDL_DestroyAudioStream(stream_);
}

void SoundLoop::play(bool on) noexcept {
    playing_.store(on, std::memory_order_relaxed);
}

void SDLCALL SoundLoop::more(void* loop, SDL_AudioStream* stream, int wanted, int /*total*/) {
    static_cast<SoundLoop*>(loop)->feed(stream, wanted);
}

void SoundLoop::feed(SDL_AudioStream* stream, int wanted) {
    if (!playing()) {
        return;
    }
    const std::size_t size = clip_.pcm.size();
    auto left = static_cast<std::size_t>(std::max(wanted, 0));
    while (left > 0) {
        const std::size_t part = std::min(left, size - at_);
        if (!SDL_PutAudioStreamData(stream, clip_.pcm.data() + at_, static_cast<int>(part))) {
            return;
        }
        at_ = (at_ + part) % size;
        left -= part;
    }
}

} // namespace opensu::audio
