#include "host_api.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <map>
#include <stdexcept>
#include <system_error>
#include <utility>
#include <vector>

#include "lauxlib.h"
#include "library/titles.hpp"
#include "lua_json.hpp"
#include "lucent/log.h"

namespace opensu::plugin {
namespace {

namespace fs = std::filesystem;

/// The most names `opensu.fs.list` returns.
constexpr std::size_t listLimit = 10'000;

HostContext& contextOf(lua_State* state) {
    return *static_cast<HostContext*>(lua_touserdata(state, lua_upvalueindex(1)));
}

/// A binding with C++ failures turned into Lua errors, so none crosses Lua's frames.
template <lua_CFunction Body> int guarded(lua_State* state) {
    std::string failure;
    try {
        return Body(state);
    } catch (const std::exception& thrown) {
        failure = thrown.what();
    }
    return luaL_error(state, "%s", failure.c_str());
}

std::string checkedText(lua_State* state, int index) {
    std::size_t size = 0;
    const char* text = luaL_checklstring(state, index, &size);
    return {text, size};
}

Pusher failure(std::string message) {
    return [message = std::move(message)](lua_State* state) {
        lua_pushnil(state);
        lua_pushlstring(state, message.data(), message.size());
        return 2;
    };
}

int logAt(lua_State* state, lucent::Level level) {
    const std::string text = checkedText(state, 1);
    lucent::log(level, contextOf(state).channel, text);
    return 0;
}

int logInfo(lua_State* state) {
    return logAt(state, lucent::Level::Info);
}

int logWarn(lua_State* state) {
    return logAt(state, lucent::Level::Warn);
}

int logError(lua_State* state) {
    return logAt(state, lucent::Level::Error);
}

int jsonDecode(lua_State* state) {
    const std::string text = checkedText(state, 1);
    std::string error;
    if (pushJson(state, text, contextOf(state).jsonNull, error)) {
        return 1;
    }
    return failure(error)(state);
}

int jsonEncode(lua_State* state) {
    luaL_checkany(state, 1);
    std::string text;
    std::string error;
    if (!encodeJson(state, 1, contextOf(state).jsonNull, text, error)) {
        return failure(error)(state);
    }
    lua_pushlstring(state, text.data(), text.size());
    return 1;
}

int titleKey(lua_State* state) {
    const std::string key = library::titleKey(checkedText(state, 1));
    lua_pushlstring(state, key.data(), key.size());
    return 1;
}

int fsRead(lua_State* state) {
    std::string path = checkedText(state, 1);
    return LuaRuntime::of(state).await(
        state, [permissions = contextOf(state).permissions, path = std::move(path)]() -> Pusher {
            std::string error;
            const std::optional<fs::path> file = permissions.readable(path, error);
            if (!file) {
                return failure(error);
            }
            std::error_code ec;
            const std::uintmax_t size = fs::file_size(*file, ec);
            if (ec) {
                return failure(path + ": " + ec.message());
            }
            if (size > fileReadLimit) {
                return failure(path + " is over " + std::to_string(fileReadLimit) + " bytes");
            }
            std::ifstream in{*file, std::ios::binary};
            std::string bytes(size, '\0');
            in.read(bytes.data(), static_cast<std::streamsize>(size));
            if (!in && !in.eof()) {
                return failure("cannot read " + path);
            }
            bytes.resize(static_cast<std::size_t>(in.gcount()));
            return [bytes = std::move(bytes)](lua_State* resumed) {
                lua_pushlstring(resumed, bytes.data(), bytes.size());
                return 1;
            };
        });
}

int fsStat(lua_State* state) {
    std::string path = checkedText(state, 1);
    return LuaRuntime::of(state).await(
        state, [permissions = contextOf(state).permissions, path = std::move(path)]() -> Pusher {
            std::string error;
            const std::optional<fs::path> file = permissions.readable(path, error);
            if (!file) {
                return failure(error);
            }
            std::error_code ec;
            const fs::file_status status = fs::status(*file, ec);
            if (ec || !fs::exists(status)) {
                return failure(path + " does not exist");
            }
            const bool directory = fs::is_directory(status);
            const std::uintmax_t size = directory ? 0 : fs::file_size(*file, ec);
            const fs::file_time_type written = fs::last_write_time(*file, ec);
            if (ec) {
                return failure(path + ": " + ec.message());
            }
            const auto mtime = std::chrono::duration_cast<std::chrono::nanoseconds>(
                                   std::chrono::file_clock::to_sys(written).time_since_epoch())
                                   .count();
            return [size, mtime, directory](lua_State* resumed) {
                lua_createtable(resumed, 0, 3);
                lua_pushinteger(resumed, static_cast<lua_Integer>(size));
                lua_setfield(resumed, -2, "size");
                lua_pushinteger(resumed, static_cast<lua_Integer>(mtime));
                lua_setfield(resumed, -2, "mtime");
                lua_pushstring(resumed, directory ? "directory" : "file");
                lua_setfield(resumed, -2, "kind");
                return 1;
            };
        });
}

int fsList(lua_State* state) {
    std::string path = checkedText(state, 1);
    return LuaRuntime::of(state).await(
        state, [permissions = contextOf(state).permissions, path = std::move(path)]() -> Pusher {
            std::string error;
            const std::optional<fs::path> dir = permissions.readable(path, error);
            if (!dir) {
                return failure(error);
            }
            std::vector<std::string> names;
            std::error_code ec;
            for (fs::directory_iterator it{*dir, ec}, end; !ec && it != end; it.increment(ec)) {
                if (names.size() == listLimit) {
                    return failure(path + " holds over " + std::to_string(listLimit) + " entries");
                }
                names.push_back(it->path().filename().string());
            }
            if (ec) {
                return failure(path + ": " + ec.message());
            }
            std::ranges::sort(names);
            return [names = std::move(names)](lua_State* resumed) {
                lua_createtable(resumed, static_cast<int>(names.size()), 0);
                lua_Integer at = 1;
                for (const std::string& name : names) {
                    lua_pushlstring(resumed, name.data(), name.size());
                    lua_rawseti(resumed, -2, at++);
                }
                return 1;
            };
        });
}

/// The `headers` field of the options table at `index` as "Name: value" lines.
std::vector<std::string> requestHeaders(lua_State* state, int index) {
    std::vector<std::string> headers;
    if (lua_isnoneornil(state, index)) {
        return headers;
    }
    luaL_checktype(state, index, LUA_TTABLE);
    lua_pushstring(state, "headers");
    if (lua_rawget(state, index) == LUA_TNIL) {
        lua_pop(state, 1);
        return headers;
    }
    luaL_checktype(state, -1, LUA_TTABLE);
    lua_pushnil(state);
    while (lua_next(state, -2) != 0) {
        if (lua_type(state, -2) != LUA_TSTRING || lua_type(state, -1) != LUA_TSTRING) {
            luaL_error(state, "headers map names to strings");
        }
        const std::string name = lua_tostring(state, -2);
        const std::string value = lua_tostring(state, -1);
        if (name.empty() || name.find_first_of(":\r\n") != std::string::npos ||
            value.find_first_of("\r\n") != std::string::npos) {
            luaL_error(state, "header %s is not a valid header", name.c_str());
        }
        std::string header = name;
        header += ": ";
        header += value;
        headers.push_back(std::move(header));
        lua_pop(state, 1);
    }
    lua_pop(state, 1);
    return headers;
}

int httpGet(lua_State* state) {
    std::string url = checkedText(state, 1);
    std::vector<std::string> headers = requestHeaders(state, 2);
    const HostContext& context = contextOf(state);
    if (std::string refused = context.permissions.urlRefusal(url); !refused.empty()) {
        return failure(refused)(state);
    }
    return LuaRuntime::of(state).await(
        state,
        [permissions = context.permissions, web = context.web, url = std::move(url),
         headers = std::move(headers)]() -> Pusher {
            const net::WebClient::Guard guard{.maxBody = httpBodyLimit,
                                              .refusal = [&permissions](const std::string& hop) {
                                                  return permissions.urlRefusal(hop);
                                              }};
            std::string error;
            std::optional<net::WebClient::Answer> answer = web->request(url, headers, guard, error);
            if (!answer) {
                return failure(error);
            }
            return [answer = std::move(*answer)](lua_State* resumed) {
                lua_createtable(resumed, 0, 2);
                lua_pushinteger(resumed, static_cast<lua_Integer>(answer.status));
                lua_setfield(resumed, -2, "status");
                lua_pushlstring(resumed, answer.body.data(), answer.body.size());
                lua_setfield(resumed, -2, "body");
                return 1;
            };
        });
}

/// Registers the source in `slot` from `{ claims = ..., <callback> = function }`.
int registerSource(lua_State* state, std::optional<Registration> HostContext::* slot,
                   const char* callback) {
    HostContext& context = contextOf(state);
    if (!context.loading) {
        return luaL_error(state, "sources can be registered only while main.lua loads");
    }
    if ((context.*slot).has_value()) {
        return luaL_error(state, "this kind of source is already registered");
    }
    luaL_checktype(state, 1, LUA_TTABLE);
    lua_pushstring(state, "claims");
    lua_rawget(state, 1);
    Registration registration{LUA_NOREF, readClaims(state, -1)};
    lua_pop(state, 1);
    lua_pushstring(state, callback);
    if (lua_rawget(state, 1) != LUA_TFUNCTION) {
        return luaL_error(state, "the source needs a %s function", callback);
    }
    registration.function = luaL_ref(state, LUA_REGISTRYINDEX);
    context.*slot = std::move(registration);
    return 0;
}

int registerPlayTime(lua_State* state) {
    return registerSource(state, &HostContext::playTime, "read");
}

int registerGameList(lua_State* state) {
    return registerSource(state, &HostContext::gameList, "list");
}

/// Sets the functions of `functions` on the table at the top, each with the context as upvalue.
void setFunctions(lua_State* state, HostContext& context,
                  std::initializer_list<std::pair<const char*, lua_CFunction>> functions) {
    for (const auto& [name, function] : functions) {
        lua_pushlightuserdata(state, &context);
        lua_pushcclosure(state, function, 1);
        lua_setfield(state, -2, name);
    }
}

void pushReadRoots(lua_State* state, const Permissions& permissions) {
    const std::map<std::string, std::vector<fs::path>> roots = permissions.existingRoots();
    lua_createtable(state, 0, static_cast<int>(roots.size()));
    for (const auto& [group, paths] : roots) {
        lua_createtable(state, static_cast<int>(paths.size()), 0);
        lua_Integer at = 1;
        for (const fs::path& path : paths) {
            lua_pushstring(state, path.c_str());
            lua_rawseti(state, -2, at++);
        }
        lua_setfield(state, -2, group.c_str());
    }
}

} // namespace

bool installHostApi(LuaRuntime& runtime, HostContext& context, std::string& error) {
    return runtime.protect(
        [&context](lua_State* state) {
            lua_createtable(state, 0, 8);
            lua_pushinteger(state, apiVersion);
            lua_setfield(state, -2, "api_version");
            lua_pushstring(state, context.id.c_str());
            lua_setfield(state, -2, "id");
            setFunctions(state, context,
                         {{"register_playtime_source", guarded<registerPlayTime>},
                          {"register_game_list", guarded<registerGameList>}});

            lua_createtable(state, 0, 3);
            setFunctions(state, context,
                         {{"info", guarded<logInfo>},
                          {"warn", guarded<logWarn>},
                          {"error", guarded<logError>}});
            lua_setfield(state, -2, "log");

            lua_createtable(state, 0, 3);
            setFunctions(state, context,
                         {{"decode", guarded<jsonDecode>}, {"encode", guarded<jsonEncode>}});
            lua_createtable(state, 0, 0);
            lua_pushvalue(state, -1);
            context.jsonNull = luaL_ref(state, LUA_REGISTRYINDEX);
            lua_setfield(state, -2, "null");
            lua_setfield(state, -2, "json");

            lua_createtable(state, 0, 3);
            setFunctions(
                state, context,
                {{"read", guarded<fsRead>}, {"stat", guarded<fsStat>}, {"list", guarded<fsList>}});
            lua_setfield(state, -2, "fs");

            lua_createtable(state, 0, 1);
            pushReadRoots(state, context.permissions);
            lua_setfield(state, -2, "read");
            lua_setfield(state, -2, "paths");

            lua_createtable(state, 0, 1);
            setFunctions(state, context, {{"get", guarded<httpGet>}});
            lua_setfield(state, -2, "http");

            lua_createtable(state, 0, 1);
            setFunctions(state, context, {{"key", guarded<titleKey>}});
            lua_setfield(state, -2, "titles");

            lua_setglobal(state, "opensu");
        },
        error);
}

} // namespace opensu::plugin
