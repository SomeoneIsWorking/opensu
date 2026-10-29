// steam_host — where the Steam client lives on this OS and how its games show in the process table.
// One backend per OS, picked in CMakeLists.txt: the home folder's Steam roots and Steam's reaper on
// Linux (`steam_host_linux.cpp`); the registry's SteamPath and the game's install folder on Windows
// (`steam_host_win32.cpp`); Application Support and the install folder on macOS
// (`steam_host_darwin.cpp`); none on Android (`steam_host_none.cpp`).
#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace opensu::library::steam {

/// The places Steam installs itself for the user whose home is `home`, in the order Steam itself
/// prefers; each may be missing.
[[nodiscard]] std::vector<std::filesystem::path> standardRoots(const std::filesystem::path& home);

/// What a running game's command line holds, for `ProcessTree`: Linux runs every game, native or
/// Proton, under `reaper SteamLaunch AppId=<id>`; Windows and macOS start the game's own executable
/// from its install folder, `steamapps/common/<installdir>`.
[[nodiscard]] std::string processHint(const std::string& appId, const std::string& installDir);

} // namespace opensu::library::steam
