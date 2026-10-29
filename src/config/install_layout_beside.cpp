#include <string>

#include <SDL3/SDL_filesystem.h>

#include "config.hpp"
#include "install_layout.hpp"

namespace opensu::config {
namespace {

std::filesystem::path shellExecutable() {
    const char* base = SDL_GetBasePath();
    if (base == nullptr) {
        return {};
    }
    return std::filesystem::path{base} / (std::string{"opensu"} + OPENSU_EXECUTABLE_SUFFIX);
}

} // namespace

InstallLayout installLayout(std::string_view assets) {
    const std::filesystem::path self = shellExecutable();
    return InstallLayout{
        .executable = self,
        .assetsDir = assets.empty() ? self.parent_path().parent_path() / OPENSU_ASSETS_RELATIVE
                                    : std::filesystem::path{assets},
        .gamescope = gamescopeBeside(self),
        .retroRunner = retroRunnerBeside(self),
        .librashader = librashaderBeside(self),
    };
}

} // namespace opensu::config
