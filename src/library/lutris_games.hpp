// lutris_games — the games Lutris has installed, read only from its `pga.db` (SQLite, opened
// immutable so nothing is written beside it), as the player's own games to import. Lutris runs
// them (`lutris lutris:rungameid/<id>`) with its own runner and settings.
#pragma once

#include <filesystem>
#include <vector>

#include "own_games.hpp"

namespace opensu::library::own {

/// Lutris's installed games under its data folder (`lutris::dataDir`), by name; nothing when it has
/// no database. Throws when the database is there but cannot be read.
[[nodiscard]] std::vector<Entry> lutrisGames(const std::filesystem::path& lutrisData);

} // namespace opensu::library::own
