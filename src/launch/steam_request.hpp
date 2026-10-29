// steam_request — hands a Steam game to the Steam client. One backend per OS, picked in
// CMakeLists.txt: `steam -applaunch <id>` on Linux (`steam_request_applaunch.cpp`), which passes
// the request to the running client and exits; the `steam://rungameid/<id>` link through the
// Windows shell (`steam_request_url.cpp`) or macOS's `open` (`steam_request_open.cpp`), which start
// Steam when it is not running, outside any group of openSU's.
#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "library/game.hpp"

namespace opensu::launch {

/// Asks Steam to run `game`, looking its launcher up in `executablePath` where there is one.
/// False with `failure` when the request could not be made.
[[nodiscard]] bool requestSteamRun(const library::Game& game,
                                   const std::vector<std::filesystem::path>& executablePath,
                                   std::string& failure);

} // namespace opensu::launch
