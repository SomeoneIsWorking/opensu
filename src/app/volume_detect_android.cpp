#include "audio_manager_volume.hpp"
#include "volume_control.hpp"

namespace opensu::app {

std::unique_ptr<audio::VolumeBackend> VolumeControl::detect(const config::Config& /*config*/,
                                                            bool readOnly) {
    std::unique_ptr<audio::VolumeBackend> backend = audio::makeAudioManager();
    if (readOnly) {
        return audio::makeReadOnly(std::move(backend));
    }
    return backend;
}

} // namespace opensu::app
