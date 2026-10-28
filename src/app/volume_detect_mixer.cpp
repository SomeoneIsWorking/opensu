#include "launch/command.hpp"
#include "volume_control.hpp"

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

} // namespace opensu::app
