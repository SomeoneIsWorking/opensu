// core_target — the libretro builds this openSU runs: the buildbot's nightly folder for its OS and
// CPU and the cores' library suffix there. One value per build, from the target CMakeLists.txt
// names (`OPENSU_CORE_BUILDBOT_FOLDER`, `OPENSU_CORE_LIBRARY_SUFFIX`).
#pragma once

#include <string_view>

namespace opensu::library::roms {

struct CoreTarget {
    /// The buildbot folder shipped cores are downloaded from, ending in /.
    std::string_view buildbotFolder;
    /// A core's library suffix: ".so", ".dll", ".dylib" or Android's "_android.so".
    std::string_view librarySuffix;
};

inline constexpr CoreTarget coreTarget{
    .buildbotFolder = OPENSU_CORE_BUILDBOT_FOLDER,
    .librarySuffix = OPENSU_CORE_LIBRARY_SUFFIX,
};

} // namespace opensu::library::roms
