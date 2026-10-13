// host_api — the `opensu` table, version 1: what a plugin's Lua can call. Registration
// (`register_playtime_source`, `register_game_list`) works only while `main.lua` loads; the
// services are `log`, `json`, `fs` (read, stat, list: they wait on the I/O pool), `paths.read`,
// `http.get` (waits) and `titles.key`. Files and hosts are checked against the plugin's
// `Permissions` before anything is touched.
#pragma once

#include <optional>
#include <string>

#include "lua_runtime.hpp"
#include "net/web_client.hpp"
#include "permissions.hpp"
#include "sources.hpp"

namespace opensu::plugin {

/// The largest file `opensu.fs.read` returns and the largest HTTP body `opensu.http.get` takes.
inline constexpr std::size_t fileReadLimit = std::size_t{16} << 20U;
inline constexpr std::size_t httpBodyLimit = std::size_t{8} << 20U;

/// What the `opensu` table acts for: one plugin. Lives on its worker as long as the runtime.
struct HostContext {
    std::string id;
    /// Lucent's channel for the plugin's log lines, `plugin.<id>`.
    std::string channel;
    Permissions permissions;
    const net::WebClient* web{nullptr};
    /// Whether `main.lua` is still loading, the only time a source can be registered.
    bool loading{true};
    std::optional<Registration> playTime;
    std::optional<Registration> gameList;
    /// The registry reference of `opensu.json.null`.
    int jsonNull{LUA_NOREF};
};

/// Sets the `opensu` global in `runtime` for `context`. False with `error` when it cannot.
bool installHostApi(LuaRuntime& runtime, HostContext& context, std::string& error);

} // namespace opensu::plugin
