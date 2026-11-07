// core_target — the libretro builds this openSU runs: the buildbot's nightly folder for its OS and
// CPU, the cores' library suffix there and the build's name. One value per build, from the target
// CMakeLists.txt names (`OPENSU_CORE_BUILDBOT_FOLDER`, `OPENSU_CORE_LIBRARY_SUFFIX`,
// `OPENSU_CORE_BUILD`).
#pragma once

#include <string_view>

namespace opensu::library::roms {

struct CoreTarget {
    /// The buildbot folder shipped cores are downloaded from, ending in /.
    std::string_view buildbotFolder;
    /// A core's library suffix: ".so", ".dll", ".dylib" or Android's "_android.so".
    std::string_view librarySuffix;
    /// The OS and CPU built for, as a catalog core release names its assets: "linux-x86_64",
    /// "windows-x86_64", "macos-arm64", "android-arm64-v8a" or "android-x86_64".
    std::string_view build;
};

inline constexpr CoreTarget coreTarget{
    .buildbotFolder = OPENSU_CORE_BUILDBOT_FOLDER,
    .librarySuffix = OPENSU_CORE_LIBRARY_SUFFIX,
    .build = OPENSU_CORE_BUILD,
};

} // namespace opensu::library::roms
