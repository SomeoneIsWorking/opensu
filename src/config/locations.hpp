// locations — where openSU keeps its settings, its data, its cache and its per-login files on this
// OS, by Lucent's per-OS resolvers (`lucent/platform.h`). On Linux they are the XDG directories'
// `opensu` folders; the folder name is the build's (`OPENSU_LOCATION_NAME`, set in CMakeLists.txt).
#pragma once

#include <filesystem>

#include "lucent/platform.h"

namespace opensu::config {

struct Locations {
    std::filesystem::path configDir;
    std::filesystem::path dataDir;
    std::filesystem::path cacheDir;
    std::filesystem::path runtimeDir;

    /// The four directories for `environment`. Throws std::runtime_error naming the one the
    /// environment leaves unresolved (no HOME, for instance).
    [[nodiscard]] static Locations resolve(const lucent::platform::Environment& environment);
};

} // namespace opensu::config
