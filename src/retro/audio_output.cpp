#include "audio_output.hpp"

#include <stdexcept>
#include <string>

#include <SDL3/SDL_audio.h>
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_init.h>

namespace opensu::retro {

DeviceOutput::DeviceOutput(AudioSink& sink) : sink_{sink} {
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        throw std::runtime_error{std::string{"audio: "} + SDL_GetError()};
    }
    const SDL_AudioSpec spec{SDL_AUDIO_S16, 2, sink.outputRate()};
    stream_ = SDL_OpenAudioDeviceStream(
        SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec,
        [](void* self, SDL_AudioStream* stream, int additional, int /*total*/) {
            static_cast<DeviceOutput*>(self)->feed(stream, additional);
        },
        this);
    if (stream_ == nullptr) {
        const std::string reason = SDL_GetError();
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        throw std::runtime_error{"audio device: " + reason};
    }
    SDL_ResumeAudioStreamDevice(stream_);
}

DeviceOutput::~DeviceOutput() {
    SDL_DestroyAudioStream(stream_);
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

void DeviceOutput::service(Clock::time_point /*now*/) {
}

void DeviceOutput::feed(SDL_AudioStream* stream, int bytes) {
    if (bytes <= 0) {
        return;
    }
    buffer_.resize(static_cast<std::size_t>(bytes) / sizeof(std::int16_t) / 2 * 2);
    sink_.pull(buffer_);
    SDL_PutAudioStreamData(stream, buffer_.data(),
                           static_cast<int>(buffer_.size() * sizeof(std::int16_t)));
}

ClockOutput::ClockOutput(AudioSink& sink) : sink_{sink} {
}

void ClockOutput::service(Clock::time_point now) {
    if (last_ != Clock::time_point{}) {
        owed_ += std::chrono::duration<double>(now - last_).count() * sink_.outputRate();
    }
    last_ = now;
    const auto frames = static_cast<std::size_t>(owed_);
    owed_ -= static_cast<double>(frames);
    sink_.discard(frames);
}

} // namespace opensu::retro
