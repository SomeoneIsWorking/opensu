#include <dlfcn.h>

#include <SDL3/SDL_system.h>

#include "install_layout.hpp"

namespace opensu::config {
namespace {

/// The APK's native library folder: the one this library was loaded from.
std::filesystem::path nativeLibraryDir() {
    Dl_info info{};
    if (dladdr(reinterpret_cast<const void*>(&nativeLibraryDir), &info) == 0 ||
        info.dli_fname == nullptr) {
        return {};
    }
    return std::filesystem::path{info.dli_fname}.parent_path();
}

} // namespace

InstallLayout installLayout(std::string_view assets) {
    const std::filesystem::path libraries = nativeLibraryDir();
    const char* files = SDL_GetAndroidInternalStoragePath();
    std::filesystem::path assetsDir{assets};
    if (assets.empty() && files != nullptr) {
        // The Activity extracts the APK's assets here once per version.
        assetsDir = std::filesystem::path{files} / "share" / "opensu";
    }
    return InstallLayout{
        .executable = {},
        .assetsDir = assetsDir,
        .gamescope = {},
        .retroRunner = libraries / "libopensu-retro.so",
        .librashader = libraries / "librashader.so",
    };
}

} // namespace opensu::config
