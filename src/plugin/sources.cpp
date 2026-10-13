#include "sources.hpp"

#include <algorithm>
#include <set>
#include <stdexcept>

#include "lauxlib.h"

namespace opensu::plugin {
namespace {

void setText(lua_State* state, const char* field, const std::string& value) {
    lua_pushlstring(state, value.data(), value.size());
    lua_setfield(state, -2, field);
}

/// The raw field `name` of the table at `index` pushed; its type.
int rawField(lua_State* state, int index, const char* name) {
    const int table = lua_absindex(state, index);
    lua_pushstring(state, name);
    return lua_rawget(state, table);
}

/// The string at the top, popped; empty for nil. Throws for another type.
std::string popOptionalText(lua_State* state, const char* what) {
    std::string value;
    if (lua_type(state, -1) == LUA_TSTRING) {
        std::size_t size = 0;
        const char* text = lua_tolstring(state, -1, &size);
        value.assign(text, size);
    } else if (!lua_isnil(state, -1)) {
        throw std::runtime_error{std::string{what} + " must be a string"};
    }
    lua_pop(state, 1);
    return value;
}

void pushEntry(lua_State* state, const library::EmulatorGameEntry& entry) {
    lua_createtable(state, 0, 4);
    setText(state, "id", entry.id);
    if (!entry.title.empty()) {
        setText(state, "title", entry.title);
    }
    if (!entry.path.empty()) {
        setText(state, "path", entry.path.string());
    }
    lua_createtable(state, 0, static_cast<int>(entry.metadata.size()));
    for (const auto& [name, value] : entry.metadata) {
        setText(state, name.c_str(), value);
    }
    lua_setfield(state, -2, "metadata");
}

std::map<std::string, std::string> readMetadata(lua_State* state) {
    std::map<std::string, std::string> metadata;
    if (lua_isnil(state, -1)) {
        return metadata;
    }
    if (!lua_istable(state, -1)) {
        throw std::runtime_error{"a game list entry's metadata must be a table"};
    }
    lua_pushnil(state);
    while (lua_next(state, -2) != 0) {
        const int valueType = lua_type(state, -1);
        if (lua_type(state, -2) != LUA_TSTRING ||
            (valueType != LUA_TSTRING && valueType != LUA_TNUMBER && valueType != LUA_TBOOLEAN)) {
            throw std::runtime_error{"metadata maps names to strings, numbers or booleans"};
        }
        // The value is copied before converting, so the key lua_next reads stays a string.
        lua_pushvalue(state, -1);
        std::size_t size = 0;
        const char* text = valueType == LUA_TBOOLEAN
                               ? (lua_toboolean(state, -1) != 0 ? "true" : "false")
                               : lua_tolstring(state, -1, &size);
        if (valueType == LUA_TBOOLEAN) {
            size = std::char_traits<char>::length(text);
        }
        metadata[lua_tostring(state, -3)] = std::string{text, size};
        lua_pop(state, 2);
    }
    return metadata;
}

} // namespace

bool Claims::covers(const library::Game& game) const {
    return game.source == library::Source::Rom &&
           std::ranges::find(systems, game.sourceId) != systems.end();
}

Claims readClaims(lua_State* state, int index) {
    luaL_checktype(state, index, LUA_TTABLE);
    Claims claims;
    if (rawField(state, index, "source") != LUA_TSTRING ||
        std::string_view{lua_tostring(state, -1)} != "rom") {
        luaL_error(state, "claims.source must be \"rom\" in API 1");
    }
    lua_pop(state, 1);
    if (rawField(state, index, "systems") != LUA_TTABLE) {
        luaL_error(state, "claims.systems must be a list of system names");
    }
    const auto count = static_cast<lua_Integer>(lua_rawlen(state, -1));
    for (lua_Integer at = 1; at <= count; ++at) {
        if (lua_rawgeti(state, -1, at) != LUA_TSTRING) {
            luaL_error(state, "claims.systems must be a list of system names");
        }
        claims.systems.emplace_back(lua_tostring(state, -1));
        lua_pop(state, 1);
    }
    lua_pop(state, 1);
    if (claims.systems.empty()) {
        luaL_error(state, "claims.systems names no system");
    }
    return claims;
}

void pushGames(lua_State* state, const std::vector<library::Game>& games,
               const std::vector<library::EmulatorGameEntry>& listed,
               const std::map<std::string, std::size_t>& matches) {
    luaL_checkstack(state, 6, "pushing games");
    lua_createtable(state, static_cast<int>(games.size()), 0);
    lua_Integer at = 1;
    for (const library::Game& game : games) {
        lua_createtable(state, 0, 7);
        setText(state, "id", game.id);
        setText(state, "title", game.title);
        setText(state, "source", "rom");
        setText(state, "system", game.sourceId);
        setText(state, "path", game.file.string());
        setText(state, "emulator", game.emulator);
        if (const auto match = matches.find(game.id); match != matches.end()) {
            pushEntry(state, listed.at(match->second));
            lua_setfield(state, -2, "listed");
        }
        lua_rawseti(state, -2, at++);
    }
}

std::optional<library::PlayTimeReports::Report>
readPlayTime(lua_State* state, int index, const std::vector<library::Game>& games) {
    if (lua_isnil(state, index)) {
        return std::nullopt;
    }
    if (!lua_istable(state, index)) {
        throw std::runtime_error{"read must return a table of game ids, or nil"};
    }
    std::set<std::string_view> given;
    for (const library::Game& game : games) {
        given.insert(game.id);
    }
    library::PlayTimeReports::Report report;
    const int table = lua_absindex(state, index);
    lua_pushnil(state);
    while (lua_next(state, table) != 0) {
        if (lua_type(state, -2) != LUA_TSTRING || !lua_istable(state, -1)) {
            throw std::runtime_error{"read must map game ids to { seconds = n }"};
        }
        std::string id = lua_tostring(state, -2);
        if (!given.contains(id)) {
            throw std::runtime_error{"read reported a game it was not given: " + id};
        }
        int isInteger = 0;
        rawField(state, -1, "seconds");
        const lua_Integer seconds = lua_tointegerx(state, -1, &isInteger);
        if (isInteger == 0 || seconds < 0) {
            throw std::runtime_error{"seconds for " + id + " must be a whole number, 0 or more"};
        }
        report[std::move(id)] = seconds;
        lua_pop(state, 2);
    }
    return report;
}

std::vector<library::EmulatorGameEntry> readGameList(lua_State* state, int index) {
    if (!lua_istable(state, index)) {
        throw std::runtime_error{"list must return a list of entries"};
    }
    const int table = lua_absindex(state, index);
    std::vector<library::EmulatorGameEntry> entries;
    const auto count = static_cast<lua_Integer>(lua_rawlen(state, table));
    for (lua_Integer at = 1; at <= count; ++at) {
        if (lua_rawgeti(state, table, at) != LUA_TTABLE) {
            throw std::runtime_error{"a game list entry must be a table"};
        }
        library::EmulatorGameEntry entry;
        rawField(state, -1, "id");
        entry.id = popOptionalText(state, "an entry's id");
        if (entry.id.empty()) {
            throw std::runtime_error{"a game list entry needs an id"};
        }
        rawField(state, -1, "title");
        entry.title = popOptionalText(state, "an entry's title");
        rawField(state, -1, "path");
        entry.path = popOptionalText(state, "an entry's path");
        rawField(state, -1, "metadata");
        entry.metadata = readMetadata(state);
        lua_pop(state, 2);
        entries.push_back(std::move(entry));
    }
    return entries;
}

} // namespace opensu::plugin
