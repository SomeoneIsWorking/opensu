// own_games_command — `opensu games`: the player's own games from a terminal or another tool. It
// adds a program, script, AppImage, `.exe` or shortcut as "Add a game" does (`own::pickedEntry`,
// titled from the files without the store), lists the entries and removes one, in the records the
// shell reads on its next listing. Adding a file already added keeps its entry, retitled when a
// title is given, so a project's install step can run it every time.
#pragma once

#include <ostream>
#include <span>
#include <string>
#include <string_view>

#include "own_games.hpp"

namespace opensu::library::own {

/// The word that starts the command.
inline constexpr std::string_view gamesCommand = "games";

/// What `opensu games` takes, for its refusals and the shell's help.
inline constexpr std::string_view gamesUsage = "  opensu games add FILE [--title TITLE]\n"
                                               "  opensu games list\n"
                                               "  opensu games remove own:ID|FILE\n";

/// Runs `args`, the words after `games`, on `records`: what was done on `out` (`own:<id>`, a tab
/// and the title, a tab and the file when listing), a refusal on `err`; the exit status.
[[nodiscard]] int runGamesCommand(std::span<const std::string> args, const Records& records,
                                  std::ostream& out, std::ostream& err);

} // namespace opensu::library::own
