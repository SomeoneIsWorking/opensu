// steam_shortcuts — Steam's non-Steam shortcuts, read only from each user's
// `<Steam root>/userdata/<user>/config/shortcuts.vdf` (binary KeyValues), as the player's own games
// to import: the shortcut's name, its program (`Exe`), folder (`StartDir`) and launch options.
// openSU runs the program itself; it never writes Steam's files.
#pragma once

#include <filesystem>
#include <string_view>
#include <vector>

#include "own_games.hpp"

namespace opensu::library::own {

/// `options` as words, split at blanks outside double quotes; the words after `%command%` when it
/// is there, since what comes before it wraps Steam's own launch.
[[nodiscard]] std::vector<std::string> launchWords(std::string_view options);

/// Every shortcut with a name and a program of every user under `steamRoots`, once per app id.
[[nodiscard]] std::vector<Entry>
steamShortcuts(const std::vector<std::filesystem::path>& steamRoots);

} // namespace opensu::library::own
