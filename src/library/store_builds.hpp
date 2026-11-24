// store_builds — which of a store's builds this OS runs: its own (GOG's `linux`, `windows` or
// `osx`), and a Windows build natively (Windows) or under Wine (Linux, macOS); and which one an
// install takes, this OS's own unless the player picked the Windows one for the game. One set of
// values per build, from src/library/CMakeLists.txt.
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
/// This OS as a game's version names it ("Linux · AppImage").
inline constexpr std::string_view ownSystemName = gogOwnPlatform == "linux" ? "Linux"
                                                  : gogOwnPlatform == "osx" ? "macOS"
                                                                            : "Windows";
/// The builds a GOG install can take here, as a refusal names them.
inline constexpr std::string_view gogPlatformsText = OPENSU_GOG_PLATFORMS_TEXT;

/// Whether a game with `builds` has a build of this OS's own.
[[nodiscard]] constexpr bool hasOwnBuild(const Game::Builds& builds) {
    return gogOwnPlatform == "linux" ? builds.linuxNative
           : gogOwnPlatform == "osx" ? builds.mac
                                     : builds.windows;
}

/// Whether the player chooses between a game's own build and its Windows one under Wine: it has
/// both, and Windows builds are not this OS's own.
[[nodiscard]] constexpr bool offersBuildChoice(const Game::Builds& builds) {
    return !windowsBuildsNative && windowsBuildsRun && builds.windows && hasOwnBuild(builds);
}

/// The gogdl platform a GOG install of a game with `builds` takes: the Windows build when
/// `windowsPicked` and the game offers the choice, else this OS's own, else a Windows one where
/// those run; empty when it has neither.
[[nodiscard]] constexpr std::string_view gogInstallPlatform(const Game::Builds& builds,
                                                            bool windowsPicked) {
    if (windowsPicked && offersBuildChoice(builds)) {
        return "windows";
    }
    if (hasOwnBuild(builds)) {
        return gogOwnPlatform;
    }
    if (builds.windows && windowsBuildsRun) {
        return "windows";
    }
    return {};
}

} // namespace opensu::library
