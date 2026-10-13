# Lua plugins

Status: phase 1 built (API v1: play-time sources, emulator game lists, the bundled Eden play-time
plugin). Phases 2 and 3 below are proposals.

## 1. What it owns

`src/plugin/` (`opensu_plugin`) runs sandboxed Lua 5.4 plugins described by a JSON manifest. Each
plugin has one `lua_State` on its own worker thread; the frame loop never runs Lua. A plugin
answers the host through a versioned `opensu` table. `src/app/plugin_service` composes the host
into the shell, and `src/app/plugin_settings` is the Settings › Plugins page.

| Owner | Owns |
| --- | --- |
| `cmake/Lua.cmake` | `opensu_lua`: Lua 5.4.9 from the lua.org tarball (FetchContent, SHA-256 pinned), every source compiled as C++, only the core plus the base, string, table, math and utf8 libraries |
| `plugin/manifest` | `plugin.json` parsing and validation, path-token expansion, the permission list a user grants |
| `plugin/permissions` | Read-path checks (absolute, `weakly_canonical`, component prefix) and URL checks (HTTPS, no credentials, declared host) |
| `plugin/lua_runtime` | One sandboxed state: allocator cap, count hook, opened libraries, protected calls, coroutine activations that yield on I/O |
| `plugin/lua_json` | JSON between Lua values and nlohmann, depth capped at 64 |
| `plugin/sources` | The Lua shape of a game, a game-list entry and a play-time report; source claims |
| `plugin/host_api` | The `opensu` table |
| `plugin/io_pool` | Two threads shared by all plugins for file reads and HTTP |
| `plugin/plugin_worker` | One plugin's thread: load, refresh (list, match, read), fault counting |
| `plugin/plugin_host` | Discovery of bundled and user plugin dirs, start/stop, `take()` of plain C++ results |
| `library/play_time` | `PlayTimeReports`: the merge of every source's report into `Game::playtimeMinutes` |
| `library/emulator_game_list` | `matchGameList`: an emulator's game list against openSU's games |
| `net/web_client` | `request()` with a body cap and a per-hop redirect check |
| `settings/settings` | `plugins`: `{id: {enabled, granted}}` |
| `app/plugin_service` | Which plugins run, refresh requests, reports applied to the library, stop toasts |
| `app/plugin_settings` | Settings › Plugins rows and the consent dialog |
| `assets/plugins/eden-playtime/` | The bundled Eden plugin |

Rules on what a plugin may be (unchanged from the proposal): Lua never holds a secret, never starts
a process, never touches the window and never runs per frame. Steam, GOG, ROM scanning, artwork
fetchers, launch and sign-in stay C++.

## 2. Data flow

```
ShellApp ── applyCatalog / game ended ──▶ PluginService::request(games)
                                              │
                                              ▼
                                  PluginHost::refresh(games) ──▶ PluginWorker (one per plugin)
                                                                   1. list()          (game list)
                                                                   2. matchGameList   (C++)
                                                                   3. read(games)     (play time)
                                                                   fs/http → IoPool, coroutine waits
ShellApp loop ── PluginService::service() ◀── PluginHost::take() ◀── Outbox (plain C++ news)
                     │
                     ├─ PlayTimeReports::set(plugin, report) → apply(games) → shelf refresh
                     └─ stopped plugin → toast "<name> stopped: <reason>", Settings refresh
```

- The frame loop posts requests and takes plain C++ results, the `CatalogLoader` pattern.
- A refresh is asked for when the catalog is applied and when a game session ends.
- A `lua_State` is touched only by its worker. File reads and HTTP run on the I/O pool, and the
  coroutine that asked is resumed on its worker with the result.
- A refresh asked for while one runs is queued; only the newest game list is kept.

## 3. Host API, version 1

`opensu.api_version == 1`. A manifest whose `api` is not 1 is refused.

### Registration (only while `main.lua` runs)

```lua
opensu.register_game_list{
  claims = { source = "rom", systems = { "switch" } },
  list = function() ... end,  -- returns { { id, title?, path?, metadata = { [k] = "v" } }, ... }
}
opensu.register_playtime_source{
  claims = { source = "rom", systems = { "switch" } },
  read = function(games) ... end,  -- returns { [game.id] = { seconds = n } }, or nil
}
```

Each kind can be registered once. A plugin that registers neither is stopped.

The **emulator game list** is the emulator's own record of the games it knows: its ID for each
game, plus whatever title, path and metadata it keeps. The host matches it to openSU's games in C++
(`library::matchGameList`): first by path (lexically normal), then by `titles::titleKey` where the
key is unique on both sides. No launch is needed to match a game.

A game handed to `read` is a copy with `id`, `title`, `source`, `system`, `path` (the ROM file),
`emulator`, and `listed` (`{id, title, path, metadata}`) when its emulator entry matched. Only games
the claims cover are passed. `read` returning `nil` means "no new answer", and the host keeps the
previous report. A report naming an id that was not passed is a fault.

### Services

The calls marked "waits" suspend the calling coroutine until the I/O pool answers.

| Call | Behaviour |
| --- | --- |
| `opensu.log.info/warn/error(text)` | Lucent, channel `plugin.<id>`, one line |
| `opensu.json.decode(s)` / `encode(t)` | `nil, err` on failure; depth 64; JSON `null` is `opensu.json.null` |
| `opensu.fs.read(path)` (waits) | Bytes, or `nil, err`; capped at 16 MiB |
| `opensu.fs.stat(path)` (waits) | `{size, mtime, kind}` (`mtime` in ns, `kind` `"file"` or `"directory"`), or `nil, err` |
| `opensu.fs.list(dir)` (waits) | File names, or `nil, err` |
| `opensu.paths.read.<group>` | The manifest's read paths for that group, expanded, that exist as directories |
| `opensu.http.get(url, {headers = {...}})` (waits) | `{status, body}`, or `nil, err`; HTTPS to declared hosts only, each redirect hop checked, body capped at 8 MiB |
| `opensu.titles.key(title)` | `library::titleKey`, the one title matcher |

Every path is checked after `weakly_canonical`, so `..` and symlinks that leave a read path are
refused, as are relative paths. There is no write, `require`, or event API in v1.

## 4. Runtime and sandbox

Lua 5.4.9 compiled as C++, so `lua_error` throws and the host unwinds normally. LuaJIT was rejected
(5.1 language without 64-bit integers, JIT traces skip the count hook); 5.5 is a later bump.

- Opened: `base`, `string`, `table`, `math`, `utf8`. Never opened: `io`, `os`, `debug`,
  `package`, `coroutine`.
- Removed: `dofile`, `loadfile`, `load`, `collectgarbage`, `print`, `require`, `string.dump`.
- `main.lua` is compiled in text mode (`"t"`), so bytecode is refused. It is capped at 1 MiB.
- The binding is hand-written over the C API. Every host call into Lua goes through
  `LuaRuntime::protect` (a `pcall` trampoline), and a panic throws `LuaPanic`.

| Limit | Value | Enforced by |
| --- | --- | --- |
| Memory | 32 MiB per state | The allocator returns `NULL` (`not enough memory`) |
| Instructions | 50 M per activation | Count hook every 10 k instructions |
| Run time | 5 s per activation, time waiting on I/O excluded | The same hook, against a steady clock |
| Faults | 3 in a row | The plugin is stopped for the session; a good answer resets the count |

A fault is logged with the plugin's channel and its traceback on one line, counted, and shown in
Settings › Plugins. The stop raises one toast. The count hook also ends a plugin on shutdown.

Known gap: a long C call (a pathological `string.find` or `string.gsub` pattern)
does not reach the count hook. It stalls only that plugin's worker, never the UI.

## 5. Packaging and trust

```
<plugin dir>/
  plugin.json
  main.lua
```

```json
{
  "id": "eden-playtime",
  "name": "Eden play time",
  "version": "1.0.0",
  "api": 1,
  "permissions": {
    "read": {
      "play_time": ["{xdg_data}/eden/play_time", "{flatpak_data:dev.eden_emu.eden}/eden/play_time"],
      "game_list": ["{xdg_cache}/eden/game_list", "{flatpak_cache:dev.eden_emu.eden}/eden/game_list"]
    },
    "http": []
  }
}
```

- `id` is `[a-z0-9-]+`. Read paths are named groups so the plugin can find them in
  `opensu.paths.read`. Tokens: `{home}`, `{xdg_data}`, `{xdg_config}`, `{xdg_cache}`,
  `{flatpak_data:<app id>}` (`~/.var/app/<id>/data`), `{flatpak_cache:<app id>}`
  (`~/.var/app/<id>/cache`), or an absolute path. A pattern containing `..` is refused.
- The token values come from `config::Config` (`xdgDataHome`, `xdgConfigHome`, `xdgCacheHome`).
- `plugin.json` is read without running code and is capped at 64 KiB.

| What | Where |
| --- | --- |
| Bundled plugins | `assets/plugins/<id>/`, installed with the assets |
| User plugins | `<data dir>/plugins/<id>/` |
| Enabled flag and granted permissions | `settings.json` under `plugins` |

Trust:

- Bundled plugins run unless turned off. A user plugin with a bundled plugin's id is refused.
- A user plugin starts off. Turning it on opens the session dialog, which lists every read path and
  host in words; `granted` stores the exact permission list accepted. A new version with a different
  list is off until accepted again.
- No plugin index. Revisit when third-party plugins exist.

## 6. Eden play time

### Eden's files

From Eden's source (`src/frontend_common/play_time_manager.cpp`, `src/qt_common/game_list/worker.cpp`,
commit `218c6705d950635bc18f8458d738a1996fb592b6`):

- `<eden data>/play_time/playtime.bin` is a flat array of `{u64 program_id; u64 seconds}`,
  little-endian, one file for all users. Eden rewrites it in place every 30 s while a game runs, so
  a reader can catch it half-written. The plugin stats before and after the read and drops the read
  unless size, mtime and length agree and the length is whole records.
- `<eden cache>/game_list/` is Eden's game-list cache, keyed **only by program ID**:
  `<ID>.pv.txt` holds the update and add-on versions for each scanned game; `<ID>.appname.txt` and
  `<ID>.jpeg` exist only for games installed to Eden's NAND. **No file path is stored anywhere**
  (nor in `launched.json`; `qt-config.ini`'s recent files have paths but no IDs). So Eden's list
  gives every scanned ID, and a title only for NAND games; it cannot map a ROM path to an ID.

### Mapping a ROM to a program ID

1. **Tag in the path**: the last 16-hex-digit `[…]` tag in the ROM's path; an update ID (`…800`)
   maps to its game (`…000`).
2. **Eden's game list**: the entry `matchGameList` matched to the game (by title, for NAND games).

A game neither tagged nor titled in Eden's cache shows no play time. It is not guessed. Reading the
ID from the dump itself (NSP: the ticket name in the PFS0 header; XCI needs the user's keys) is the
next step; it needs read permission on the ROM folders.

### Merge

`library::PlayTimeReports` holds the latest report per source. For a ROM game,
`playtimeMinutes = Σ seconds / 60` over the sources that report it. Each emulator counts only its
own sessions, so summing counts each session once, and reports are keyed by source and game, so a
repeated report replaces rather than adds. openSU measures no session length of its own, so nothing
is double counted. Store games keep the store's value. Last played is unchanged.

## 7. How to test

- `tests/plugin/plugin_host_test`: sandbox escapes (`io`, `os`, `debug`, `load`, `string.dump`,
  `require`, `dofile`, `collectgarbage`, a bytecode chunk), the memory bomb, the instruction limit,
  three faults in a row, read-path refusals (undeclared, `..`, symlink, relative), HTTP refusals,
  JSON, refused manifests.
- `tests/plugin/eden_playtime_test`: the shipped `main.lua` through the host against a fixture
  `playtime.bin` and game-list cache: tag, update tag, title match, half-written file, merge.
- `tests/plugin/manifest_test`, `tests/net/web_client_test` (redirect asked per hop, refused hop,
  body cap), `tests/library/play_time_test`, `tests/app/settings_controller_test`
  (`pluginsAskBeforeTheyRun`).
- Real data: `--hidden` with scratch `XDG_CONFIG_HOME` and `XDG_DATA_HOME`, Eden's data reachable
  read-only, the control channel driving to a game's details page.

## 8. Later phases (proposal)

- **Phase 2 (API v2)**: settings rows, game actions, artwork sources (`find = fn(game) -> url`,
  host downloads). Candidates: more emulators' play time (Ryujinx, PCSX2, DuckStation; verify each
  format from source), SteamGridDB art with a key setting, an NSP ticket reader for Switch IDs.
- **Phase 3 (API v3)**: `register_library_source` and an argv-only, allow-listed `run`
  permission. First restructure `Source` into a descriptor (`Source::Plugin`, `Game::origin`), then
  move Epic's listing to a bundled plugin and delete `epic.cpp` listing in the same commit. A C++
  and a Lua version of one integration never coexist.
