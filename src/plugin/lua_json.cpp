#include "lua_json.hpp"

#include <cmath>
#include <stdexcept>

#include <nlohmann/json.hpp>

#include "lauxlib.h"

namespace opensu::plugin {
namespace {

using json = nlohmann::json;

void push(lua_State* state, const json& value, int nullReference) {
    luaL_checkstack(state, 3, "decoding JSON");
    switch (value.type()) {
    case json::value_t::null:
        lua_rawgeti(state, LUA_REGISTRYINDEX, nullReference);
        break;
    case json::value_t::boolean:
        lua_pushboolean(state, value.get<bool>() ? 1 : 0);
        break;
    case json::value_t::number_integer:
        lua_pushinteger(state, value.get<lua_Integer>());
        break;
    case json::value_t::number_unsigned:
        lua_pushinteger(state, static_cast<lua_Integer>(value.get<std::uint64_t>()));
        break;
    case json::value_t::number_float:
        lua_pushnumber(state, value.get<double>());
        break;
    case json::value_t::string: {
        const auto& text = value.get_ref<const std::string&>();
        lua_pushlstring(state, text.data(), text.size());
        break;
    }
    case json::value_t::array: {
        lua_createtable(state, static_cast<int>(value.size()), 0);
        lua_Integer at = 1;
        for (const json& item : value) {
            push(state, item, nullReference);
            lua_rawseti(state, -2, at++);
        }
        break;
    }
    case json::value_t::object:
        lua_createtable(state, 0, static_cast<int>(value.size()));
        for (const auto& [key, item] : value.items()) {
            lua_pushlstring(state, key.data(), key.size());
            push(state, item, nullReference);
            lua_rawset(state, -3);
        }
        break;
    case json::value_t::binary:
    case json::value_t::discarded:
        lua_pushnil(state);
        break;
    }
}

/// Whether the table at `index` has exactly the keys 1..n, n > 0.
bool isArray(lua_State* state, int index) {
    const lua_Unsigned length = lua_rawlen(state, index);
    if (length == 0) {
        return false;
    }
    lua_Unsigned keys = 0;
    lua_pushnil(state);
    while (lua_next(state, index) != 0) {
        lua_pop(state, 1);
        ++keys;
        int isInteger = 0;
        const lua_Integer key = lua_tointegerx(state, -1, &isInteger);
        if (lua_type(state, -1) != LUA_TNUMBER || isInteger == 0 || key < 1 ||
            static_cast<lua_Unsigned>(key) > length) {
            lua_pop(state, 1);
            return false;
        }
    }
    return keys == length;
}

json toJson(lua_State* state, int index, int nullReference, int depth) {
    if (depth > jsonDepthLimit) {
        throw std::runtime_error{"nested deeper than " + std::to_string(jsonDepthLimit)};
    }
    index = lua_absindex(state, index);
    switch (lua_type(state, index)) {
    case LUA_TNIL:
        return nullptr;
    case LUA_TBOOLEAN:
        return lua_toboolean(state, index) != 0;
    case LUA_TNUMBER:
        if (lua_isinteger(state, index) != 0) {
            return lua_tointeger(state, index);
        }
        if (!std::isfinite(lua_tonumber(state, index))) {
            throw std::runtime_error{"JSON has no infinity or NaN"};
        }
        return lua_tonumber(state, index);
    case LUA_TSTRING: {
        std::size_t size = 0;
        const char* text = lua_tolstring(state, index, &size);
        return std::string{text, size};
    }
    case LUA_TTABLE:
        break;
    default:
        throw std::runtime_error{std::string{"JSON cannot hold a "} + luaL_typename(state, index)};
    }
    lua_rawgeti(state, LUA_REGISTRYINDEX, nullReference);
    const bool isNull = lua_rawequal(state, index, -1) != 0;
    lua_pop(state, 1);
    if (isNull) {
        return nullptr;
    }
    luaL_checkstack(state, 3, "encoding JSON");
    if (isArray(state, index)) {
        json array = json::array();
        const auto length = static_cast<lua_Integer>(lua_rawlen(state, index));
        for (lua_Integer at = 1; at <= length; ++at) {
            lua_rawgeti(state, index, at);
            array.push_back(toJson(state, -1, nullReference, depth + 1));
            lua_pop(state, 1);
        }
        return array;
    }
    json object = json::object();
    lua_pushnil(state);
    while (lua_next(state, index) != 0) {
        if (lua_type(state, -2) != LUA_TSTRING) {
            throw std::runtime_error{"a JSON object's keys must all be strings"};
        }
        object[lua_tostring(state, -2)] = toJson(state, -1, nullReference, depth + 1);
        lua_pop(state, 1);
    }
    return object;
}

} // namespace

bool pushJson(lua_State* state, std::string_view text, int nullReference, std::string& error) {
    bool tooDeep = false;
    const json::parser_callback_t limit = [&tooDeep](int depth, json::parse_event_t, json&) {
        if (depth > jsonDepthLimit) {
            tooDeep = true;
        }
        return !tooDeep;
    };
    const json value = json::parse(text, limit, false);
    if (tooDeep) {
        error = "nested deeper than " + std::to_string(jsonDepthLimit);
        return false;
    }
    if (value.is_discarded()) {
        error = "not JSON";
        return false;
    }
    push(state, value, nullReference);
    return true;
}

bool encodeJson(lua_State* state, int index, int nullReference, std::string& out,
                std::string& error) {
    try {
        out = toJson(state, index, nullReference, 0).dump();
        return true;
    } catch (const std::exception& thrown) {
        error = thrown.what();
        return false;
    }
}

} // namespace opensu::plugin
