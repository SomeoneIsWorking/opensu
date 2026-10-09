#include "sound_player.hpp"

#include "lucent/log.h"

namespace opensu::audio {
namespace {

std::size_t slot(Effect effect) {
    return static_cast<std::size_t>(effect);
}

} // namespace

SoundPlayer::~SoundPlayer() {
    if (!ready_) {
        return;
    }
    for (std::size_t i = 0; i < sounds_.size(); ++i) {
        if (loaded_[i]) {
            UnloadSound(sounds_[i]);
        }
    }
    CloseAudioDevice();
}

void SoundPlayer::open() {
    if (ready_) {
        return;
    }
    InitAudioDevice();
    ready_ = IsAudioDeviceReady();
    if (!ready_) {
        lucent::warn("audio", "no audio device; UI sounds are silent");
    }
}

bool SoundPlayer::load(Effect effect, const std::filesystem::path& file) {
    if (!ready_) {
        return false;
    }
    const Sound sound = LoadSound(file.string().c_str());
    if (!IsSoundValid(sound)) {
        lucent::warn("audio", "cannot load {}", file.string());
        return false;
    }
    if (loaded_[slot(effect)]) {
        UnloadSound(sounds_[slot(effect)]);
    }
    sounds_[slot(effect)] = sound;
    loaded_[slot(effect)] = true;
    return true;
}

bool SoundPlayer::play(Effect effect) {
    if (!ready_ || !loaded_[slot(effect)] || !debounce_.admit(effect, Debounce::Clock::now())) {
        return false;
    }
    PlaySound(sounds_[slot(effect)]);
    return true;
}

} // namespace opensu::audio
