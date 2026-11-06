// lutris_games — the games Lutris has installed, read only from its `pga.db` (SQLite, opened
// immutable so nothing is written beside it), as the player's own games to import, each with the
// Steam app Lutris knows it as (its Steam service id, a Steam runner's `appid`, or the umu
// `GAMEID` in its config), which its cover and Proton fixes follow. openSU starts a Linux game or
// one on Lutris's default GE-Proton itself, from the game's config: its program, arguments, folder,
// prefix, environment and DLL overrides. Any other runner stays with Lutris
// (`lutris lutris:rungameid/<id>`).
#pragma once

#include <filesystem>
#include <vector>

#include "own_games.hpp"

namespace opensu::library::own {

/// Lutris's installed games under its data folder (`lutris::dataDir`), by name; nothing when it has
/// no database. Throws when the database is there but cannot be read.
[[nodiscard]] std::vector<Entry> lutrisGames(const std::filesystem::path& lutrisData);

} // namespace opensu::library::own
