#include "volume_control.hpp"

namespace opensu::app {

VolumeControl::VolumeControl(std::unique_ptr<audio::VolumeBackend> backend, Hooks hooks)
    : volume_{std::move(backend)}, hooks_{std::move(hooks)} {
    // The volume at start is not a change to show.
    volume_.refresh();
    volume_.setListener(hooks_.show);
}

void VolumeControl::act(input::Action action) {
    std::string refused;
    switch (action) {
    case input::Action::VolumeUp:
        refused = volume_.step(audio::volumeStep);
        break;
    case input::Action::VolumeDown:
        refused = volume_.step(-audio::volumeStep);
        break;
    case input::Action::VolumeMute:
        refused = volume_.toggleMute();
        break;
    default:
        return;
    }
    if (!refused.empty()) {
        hooks_.say(refused, true);
    }
}

} // namespace opensu::app
