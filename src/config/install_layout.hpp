// install_layout — where openSU's own files are on this OS: the shell's executable, the shipped
// assets, the Gamescope fork, the libretro runner and librashader. One backend per OS, picked in
// CMakeLists.txt: beside the executable on the desktop (`install_layout_beside.cpp`), the APK's
// native library folder and the Activity's files on Android (`install_layout_android.cpp`).
#pragma once

#include <filesystem>
#include <string_view>

namespace opensu::config {

struct InstallLayout {
    /// openSU's executable; empty where there is none (Android) or SDL cannot tell.
    std::filesystem::path executable;
    /// The shipped typeface, sounds, catalogs and plugins.
    std::filesystem::path assetsDir;
    /// The Gamescope fork; empty where there is none.
    std::filesystem::path gamescope;
    /// The libretro runner: an executable on the desktop, the `:retro` Activity's library on
    /// Android.
    std::filesystem::path retroRunner;
    /// The librashader the runner loads for a shader preset.
    std::filesystem::path librashader;
};

/// This install's layout; `assets` (OPENSU_ASSETS) replaces the shipped assets folder when set.
[[nodiscard]] InstallLayout installLayout(std::string_view assets);

} // namespace opensu::config
