// windows_builds — whether this OS runs a store's Windows build as it is: natively on Windows,
// under Wine everywhere else (`wine::prepare`). One value per build, from
// src/library/CMakeLists.txt.
#pragma once

namespace opensu::library {

inline constexpr bool windowsBuildsNative = OPENSU_WINDOWS_BUILDS_NATIVE != 0;

} // namespace opensu::library
