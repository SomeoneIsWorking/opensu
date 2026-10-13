// emulator_game_list — an emulator's own list of the games it knows, as a plugin reads it from
// the emulator's files, and which of openSU's games each entry is: the same file when the
// emulator records paths, else the same title.
#pragma once

#include <cstddef>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

#include "game.hpp"

namespace opensu::library {

/// One game in an emulator's list.
struct EmulatorGameEntry {
    /// The emulator's id for the game (a Switch program id, a disc serial).
    std::string id;
    /// The emulator's title for it; empty when the emulator has none cached.
    std::string title;
    /// The file the emulator found it in; empty when the emulator does not record it.
    std::filesystem::path path;
    /// Whatever else the emulator keeps, by name (installed update, add-ons).
    std::map<std::string, std::string> metadata;
};

/// The entry of `entries` each game of `games` is, by index, keyed by game id. A game whose file
/// is an entry's path is that entry. Otherwise a game is the entry with its title, when exactly
/// one entry and one unmatched game have that title (`titleKey`); a title shared by two is no
/// match.
[[nodiscard]] std::map<std::string, std::size_t>
matchGameList(const std::vector<Game>& games, const std::vector<EmulatorGameEntry>& entries);

} // namespace opensu::library
