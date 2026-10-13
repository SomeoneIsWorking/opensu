// sources — the two kinds of source a v1 plugin registers, and the values that cross between Lua
// and openSU for them: a play-time source (`read(games)` returns seconds per game) and an
// emulator game list (`list()` returns the emulator's own entries). Games go to Lua as plain
// tables; what comes back is checked and read into plain C++ in a protected call.
#pragma once

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "lauxlib.h"
#include "library/emulator_game_list.hpp"
#include "library/game.hpp"
#include "library/play_time.hpp"
#include "lua.h"

namespace opensu::plugin {

/// Which games a source is for. v1 sources claim ROM games of named systems.
struct Claims {
    std::vector<std::string> systems;

    [[nodiscard]] bool covers(const library::Game& game) const;
};

/// A registered callback and what it claims.
struct Registration {
    int function{LUA_NOREF};
    Claims claims;
};

/// The claims table at `index`: `{ source = "rom", systems = { "switch", ... } }`. Raises a Lua
/// error for any other shape.
[[nodiscard]] Claims readClaims(lua_State* state, int index);

/// Pushes `games` as a list of Game tables: id, title, source, system, path, emulator, and
/// `listed`, the emulator game list's entry matched to the game (`matches` maps game ids to
/// indices of `listed`).
void pushGames(lua_State* state, const std::vector<library::Game>& games,
               const std::vector<library::EmulatorGameEntry>& listed,
               const std::map<std::string, std::size_t>& matches);

/// The play-time answer at `index`: nothing for nil (no new answer), else game id to seconds from
/// `{ [id] = { seconds = n } }`. Throws for another shape or a game it was not given.
[[nodiscard]] std::optional<library::PlayTimeReports::Report>
readPlayTime(lua_State* state, int index, const std::vector<library::Game>& games);

/// The game list at `index`: a list of `{ id, title?, path?, metadata? }`. Throws for another
/// shape.
[[nodiscard]] std::vector<library::EmulatorGameEntry> readGameList(lua_State* state, int index);

} // namespace opensu::plugin
