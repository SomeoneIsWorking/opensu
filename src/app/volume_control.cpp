#include "volume_control.hpp"

#include "launch/command.hpp"

namespace opensu::app {

std::unique_ptr<audio::VolumeBackend> VolumeControl::detect(const config::Config& config,
                                                            bool readOnly) {
    const audio::Runner run =
        [](const std::string& program,
           const std::vector<std::string>& args) -> std::optional<audio::CommandOutput> {
        const std::optional<launch::Captured> out = launch::runCaptured(program, args);
        if (!out) {
            return std::nullopt;
        }
        return audio::CommandOutput{out->status, out->output};
    };
    std::unique_ptr<audio::VolumeBackend> backend =
        audio::detectBackend(run, [&config](const std::string& program) {
            return !launch::resolveExecutable(program, config.executablePath).empty();
        });
    if (backend && readOnly) {
        return audio::makeReadOnly(std::move(backend));
    }
    return backend;
}

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
