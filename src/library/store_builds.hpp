// store_builds — which of a store's builds this OS runs: its own (GOG's `linux`, `windows` or
// `osx`), and a Windows build natively (Windows), under Wine (Linux) or not at all (macOS, where
// Wine is a non-goal). One set of values per build, from src/library/CMakeLists.txt.
#pragma once

#include <string_view>

#include "game.hpp"

namespace opensu::library {

/// A store's Windows build runs as it is, without the Wine runner (`wine::prepare`).
inline constexpr bool windowsBuildsNative = OPENSU_WINDOWS_BUILDS_NATIVE != 0;
/// A store's Windows build runs here at all, natively or under Wine.
inline constexpr bool windowsBuildsRun = OPENSU_WINDOWS_BUILDS_RUN != 0;
/// gogdl's name for this OS's own builds.
inline constexpr std::string_view gogOwnPlatform = OPENSU_GOG_OWN_PLATFORM;
/// The builds a GOG install can take here, as a refusal names them.
inline constexpr std::string_view gogPlatformsText = OPENSU_GOG_PLATFORMS_TEXT;

/// The gogdl platform a GOG install of a game with `builds` takes: this OS's own build, else a
/// Windows one where those run; empty when it has neither.
[[nodiscard]] constexpr std::string_view gogInstallPlatform(const Game::Builds& builds) {
    const bool own = gogOwnPlatform == "linux" ? builds.linuxNative
                     : gogOwnPlatform == "osx" ? builds.mac
                                               : builds.windows;
    if (own) {
        return gogOwnPlatform;
    }
    if (builds.windows && windowsBuildsRun) {
        return "windows";
    }
    return {};
}

} // namespace opensu::library
