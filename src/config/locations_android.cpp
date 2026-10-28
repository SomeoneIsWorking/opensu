#include "locations.hpp"

#include <stdexcept>

#include <SDL3/SDL_system.h>

namespace opensu::config {

/// Android has no environment for these: the Activity's private files and cache directories.
Locations Locations::resolve(const lucent::platform::Environment& /*environment*/) {
    const char* files = SDL_GetAndroidInternalStoragePath();
    const char* cache = SDL_GetAndroidCachePath();
    if (files == nullptr || cache == nullptr) {
        throw std::runtime_error{std::string{"the Activity's storage paths are unavailable: "} +
                                 SDL_GetError()};
    }
    const std::filesystem::path cacheDir{cache};
    return Locations{
        .configDir = files,
        .dataDir = files,
        .cacheDir = cacheDir,
        .runtimeDir = cacheDir / "run",
    };
}

} // namespace opensu::config
