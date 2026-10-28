// audio_manager_volume — Android's media volume through the shell Activity's AudioManager. One
// output only: Android routes it to the speaker or a headset itself.
#pragma once

#include <memory>

#include "volume_backend.hpp"

namespace opensu::audio {

[[nodiscard]] std::unique_ptr<VolumeBackend> makeAudioManager();

} // namespace opensu::audio
