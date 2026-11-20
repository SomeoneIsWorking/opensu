#include <optional>
#include <string>

#include "config.hpp"
#include "install_layout.hpp"
#include "lucent/platform.h"

namespace opensu::config {
namespace {

/// Not SDL's base path, which is Contents/Resources in a macOS bundle.
std::filesystem::path shellExecutable() {
    const std::optional<std::filesystem::path> running = lucent::platform::executable_path();
    if (!running) {
        return {};
    }
    return running->parent_path() / (std::string{"opensu"} + OPENSU_EXECUTABLE_SUFFIX);
}

} // namespace

InstallLayout installLayout(std::string_view assets) {
    const std::filesystem::path self = shellExecutable();
    return InstallLayout{
        .executable = self,
        .assetsDir = assets.empty() ? self.parent_path().parent_path() / OPENSU_ASSETS_RELATIVE
                                    : std::filesystem::path{assets},
        .gamescope = gamescopeBeside(self),
        .dbusProxy = dbusProxyBeside(self),
        .retroRunner = retroRunnerBeside(self),
        .librashader = librashaderBeside(self),
    };
}

} // namespace opensu::config
