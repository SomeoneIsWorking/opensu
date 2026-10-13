# Design: Lua plugins

Status: proposal, not built. Decision wanted on section 8.

## 1. Summary

Add a sandboxed Lua 5.4 runtime that loads plugins from a JSON manifest. Each plugin runs on its
own worker thread with a hand-written, versioned `opensu` API, and the frame loop never runs Lua.
Phase 1 ships the runtime, one capability (play-time sources) and one bundled plugin, Eden play
time. No existing integration moves to Lua until the library-source API exists (phase 3). Epic's
listing then moves first, and `epic.cpp` is deleted in the same change.

## 2. What a plugin can be

The bar for "suits Lua" is high-level logic over data the host fetches: no secrets, no process
ownership, no per-file hot loops, no binary formats that need native speed.

| Integration today | Code | Verdict | Reason |
| --- | --- | --- | --- |
| Steam library | `library/steam.cpp` (516 lines), `steam/*` | C++ | Bound up with the owned Steam client, DevTools JS, the install wizard, the download queue and the launch gate. Listing alone could be Lua, but it shares state with those owners and is verified (S002). |
| Epic library | `library/epic.cpp` (147) | Lua later (phase 3) | Runs `legendary list --json` and joins two JSON lists. It takes seconds in legendary, so Lua's speed is irrelevant. It needs a "run this declared program" permission. Install (`epic_install_job`) and sign-in stay C++. |
| GOG library and sign-in | `gog*.cpp`, `gog_auth`, `gogdl_auth`, `sign_in` | C++ | OAuth tokens and owner-only writes. A token must never be readable by plugin code. |
| ROM scan | `roms.cpp`, `rom_systems.cpp` | C++ | Walks large folders, and the artwork matcher and emulator choice share its system tables. |
| Emulator rules | `emulators.cpp` table | C++ (data) | It is a constexpr table, not code. A plugin could add rules as data in a later API version, and discovery stays in C++. |
| Artwork fetchers | `artwork/*` | C++ | Range reads into APKs, zip parsing, CRC checks, the queue and the store. New sources fit Lua (for example SteamGridDB with the user's API key) when the plugin returns a URL and the host downloads it. |
| Launch, handoff, scopes | `launch/*`, `session/*` | C++ | systemd scopes, process trees, Gamescope and signals. A plugin only supplies a `LaunchSpec` through a library source. |
| Store sign-in flows | `app/sign_in`, the extension | C++ | Secrets, the control channel and the browser hand-off. |
| Play time from emulators | none today | Lua | Reads one small file per emulator and maps IDs to games. This is the ideal fit (see section 6). |

Rules that follow from this table:

- Lua code never holds a secret.
- Lua code never starts a process in v1.
- Lua code never touches the window or raylib.
- Lua code never runs per frame.

## 3. Host API, version 1

There is one global table, `opensu`. `opensu.api_version == 1`. The manifest's `api` field must
equal a version the host supports; the host supports exactly one version until a second is
needed.

### Registration

These are callable only while the plugin's `main.lua` loads.

```lua
opensu.register_playtime_source{
  claims = { source = "rom", systems = { "switch" } },  -- v1: ROM games only
  read = function(games) ... end,  -- games: list of Game; returns { [game.id] = { seconds = n } } or nil
}
opensu.on("game_started", function(game) end)  -- dispatched when a launch begins; host does not wait
opensu.on("game_ended", function(game) end)    -- after the handoff returns; then claimed sources are re-read
```

Phase 2 adds `register_artwork_source{claims, find = fn(game) -> url}`,
`register_setting{id, kind = "toggle"|"choice"|"action"|"info", label, note, ...}` and
`register_game_action{claims, label, run = fn(game)}`. Phase 3 adds
`register_library_source{id, label, list = fn() -> games}`. Each one bumps the API version, and
the new version is a superset of the old.

A `Game` handed to Lua is a plain table with these fields:

- `id`
- `title`
- `source` (`"steam"|"epic"|"gog"|"rom"`)
- `system`, for ROMs
- `path`, the ROM's file, for ROMs
- `emulator`, the chosen emulator's name

It is a copy, and writing to it changes nothing.

`read` returning `nil` means "no new answer". The host keeps the source's previous report.

### Services

These are callable only inside callbacks. The calls marked "yields" suspend the calling coroutine.

| Call | Behaviour |
| --- | --- |
| `opensu.log.info/warn/error(text)` | Lucent, tag `plugin.<id>`, one line. The text is an argument (`"{}"`), never a format string. |
| `opensu.json.decode(s)` / `encode(t)` | nlohmann. Depth is capped at 64 and decode returns `nil, err`. JSON `null` is `opensu.json.null`. |
| `opensu.fs.read(path)` (yields) | Bytes, or `nil, err`. Capped at 16 MiB. |
| `opensu.fs.stat(path)` (yields) | `{size, mtime}`, or `nil`. |
| `opensu.fs.list(dir)` (yields) | Names, or `nil, err`. |
| `opensu.fs.write(path, bytes)` (yields) | Allowed only under the plugin's data or cache dir, through `fileio::atomicWrite`. |
| `opensu.paths.data`, `.cache`, `.read_roots` | Absolute paths. `read_roots` lists the manifest's declared read paths, expanded, that exist. |
| `opensu.http.get(url, {headers = {...}})` (yields) | `{status, body}`, or `nil, err`. HTTPS only. The host must be in the manifest. Each redirect hop is checked against the same list. The body is capped at 8 MiB and the request uses `net::WebClient`'s timeouts. |
| `opensu.titles.key(title)` | `library::titles` comparison key: the one title matcher, not a Lua copy. |

The host checks paths after `weakly_canonical`, so `..` and symlink escapes are refused. The
plugin's own code dir is readable, and nothing else is unless declared.

### Threads and async

```
frame loop ──post(event / read request)──▶ PluginWorker (1 thread, 1 lua_State per plugin)
    ▲                                          │ each callback = new coroutine (lua_newthread)
    │                                          │ fs/http call → lua_yieldk, request to IoPool
    │                                          ◀─ completion posted back, coroutine resumed
    └──── take(): plain C++ results ───────────┘ (PlayTimeReport map, logs, faults)
```

- The frame loop never enters Lua. It posts requests and `take()`s plain C++ results, the same
  pattern as `CatalogLoader`.
- A `lua_State` is touched only by its plugin's worker.
- `IoPool` is two threads shared by all plugins. File reads and HTTP run there, never on a plugin
  worker, so a slow request does not stall that plugin's other callbacks.
- Interleaving happens only at yielding calls. Plugin authors are told that globals can change
  across a yielding call.

## 4. Runtime choice

| | Lua 5.4.9 | LuaJIT 2.1 | Lua 5.5.1 |
| --- | --- | --- | --- |
| License | MIT | MIT | MIT |
| Maintenance | Final 5.4 release (Aug 2026), frozen and stable | Rolling git, no releases, one maintainer | Current series (Dec 2025) |
| 64-bit integers, `string.pack/unpack` | Yes | No (Lua 5.1 language, doubles plus FFI cdata) | Yes |
| Sandboxing | `_ENV`, per-lib open, allocator, count hook | Same hooks, but the JIT ignores count hooks in compiled traces unless disabled, and the FFI must be kept out | As 5.4 |
| Compile as C++ (errors as exceptions, RAII unwinds) | Supported | No | Supported |
| Tooling (lua-language-server, luacheck) | Full | Full | Partial |

Recommendation: **Lua 5.4.9, compiled from source as C++** into an `opensu_lua` static target.

- The source comes from the checksummed lua.org tarball under `LUA_ROOT`, defaulting to
  `~/dev/lua-5.4.9`, the same pattern as `NANOSVG_ROOT`. If it is missing, the build refuses with
  the download command.
- LuaJIT's speed buys nothing for plugins that read one file. Its 5.1 language lacks the 64-bit
  integers the Eden format needs, and the JIT weakens the instruction limit.
- 5.5 is a later, mostly mechanical bump: `lua_newstate` gains a seed argument, and globals
  declarations are optional.

Binding: **hand-written** over the C API, about 400 lines in `host_api.cpp`.

- sol2's last release is 3.3.0 (2022). It is header-heavy, slows every touched translation unit,
  and adds no value for a dozen functions.
- With Lua compiled as C++, `luaL_error` throws, so bindings can hold RAII objects safely.

Sandbox:

- Libraries opened are `base`, `string`, `table`, `math`, `utf8` and `coroutine`.
- Removed from `base` are `dofile`, `loadfile`, `load`, `collectgarbage` (the host runs GC
  steps) and `print`. `print` is replaced by `opensu.log.info`.
- `string.dump` is removed, so no bytecode exists to load.
- `io`, `os`, `debug` and `package` are never opened.
- `require` is a host function that loads only `.lua` text files from the plugin's own dir, in
  text mode (`"t"`), with the plugin's `_ENV`.

Limits, each enforced per plugin:

| Limit | Value | How it is enforced |
| --- | --- | --- |
| Memory | 32 MiB | Custom `lua_Alloc` returns `NULL`, which raises `LUA_ERRMEM`. |
| Instructions | 50 M per callback activation, excluding time suspended on I/O | Count hook every 10 k instructions; `luaL_error` when exceeded. |
| Wall time | 5 s per activation | Same count hook, against a steady clock. |
| Errors | 3 consecutive faults | Disables the plugin for the session. |

The count hook also aborts the plugin on shutdown.

Error isolation:

- Every entry is `lua_resume` or `lua_pcall` with a traceback handler (`luaL_traceback`, so the
  `debug` library is not exposed).
- A fault is logged with the plugin id, counted, and shown in Settings > Plugins. Its last report
  stays in use.
- The disabling fault produces one toast ("Eden play time stopped: <reason>").
- Known gap: C library calls such as a pathological `string.find` pattern do not hit the count
  hook. They stall only that plugin's worker, never the UI. A stuck worker is reported, and
  shutdown waits for it.

## 5. Packaging and trust

```
<plugin dir>/
  plugin.json     # read without running any code
  main.lua
  *.lua           # loadable with require
```

```json
{
  "id": "eden-playtime",
  "name": "Eden play time",
  "version": "1.0.0",
  "api": 1,
  "permissions": {
    "read": ["{xdg_data}/eden/play_time", "{flatpak_data:dev.eden_emu.eden}/eden/play_time"],
    "http": []
  }
}
```

Path tokens are `{home}`, `{xdg_data}`, `{xdg_config}`, `{xdg_cache}` and `{flatpak_data:<app id>}`
(which expands to `~/.var/app/<id>/data`). `config::Config` expands them, and nothing else reads the
environment. `http` holds host names. The `id` is `[a-z0-9-]+` and must be unique.

| What | Where |
| --- | --- |
| Bundled plugins | `plugins/<id>/` in this repo, installed to `<prefix>/share/opensu/plugins/` beside the assets |
| User plugins | `<data dir>/plugins/<id>/` |
| Plugin data | `<data dir>/plugin-data/<id>/` |
| Plugin cache | `<cache dir>/plugins/<id>/` |
| Enabled flag and granted permissions | `settings.json` under `plugins`: `{id: {enabled, granted}}` |

If a user plugin has the same id as a bundled one, the user plugin is refused and a warning is
logged. A bundled plugin cannot be shadowed.

Trust model:

- Bundled plugins are reviewed with the code they ship with. They are enabled by default, with
  their manifest permissions granted.
- A user plugin appears in Settings > Plugins disabled. Enabling it opens the existing consent
  dialog (`ui/session_dialog`), which lists every read path and host in words.
- `granted` stores the exact permission set. If a new version changes it, the plugin is disabled
  until the user accepts again.

Updates:

- Bundled plugins update with openSU.
- User plugins are replaced by hand. `version` is shown in Settings, and the host compares only
  `api`.

Plugin index: **none for now.** An index means hosting, signing, review and an update channel, all
for plugins that do not exist yet. Revisit this when there are three or more third-party plugins.
The likely shape would be a JSON index in a GitHub repo, with a pinned SHA-256 per release zip and
the same consent dialog.

## 6. Worked example: Eden play time

### Where Eden keeps it

The source is `src/frontend_common/play_time_manager.cpp` on git.eden-emu.dev. The file is:

```
<eden data dir>/play_time/playtime.bin
```

The Eden data dir is `~/.local/share/eden` natively, or
`~/.var/app/dev.eden_emu.eden/data/eden` under Flatpak.

The format:

- The file is a flat array of `struct { u64 program_id; u64 seconds; }`, little-endian, 16 bytes
  per record.
- It holds one file for all users. The per-user `.bin` files are yuzu's old layout, and Eden
  has no migration from them.
- Records with `program_id == 0` are skipped.
- Eden saves every 30 s while a game runs, and on exit.
- The write truncates and rewrites in place (not atomic). A reader can catch a half-written
  file, so it checks size and mtime before and after the read.

This machine has the file at `~/.local/share/eden/play_time/playtime.bin`: 256 bytes, 16 games.
For example, `0100000000010000` (Super Mario Odyssey) has 17145 s, and `0100C1F0051B6000` has
2700 s. There is no Flatpak install.

`~/.cache/eden/launched.json` (`{program_id: {launch_count, timestamp}}`) also exists here. It was
not found in the source checked, so v1 does not use it. Last played stays openSU's own.

### Mapping a ROM to a program ID

The program ID lives inside encrypted NCAs, which need the user's keys. The plugin does not decrypt
them. It resolves the ID in two steps:

1. **Tag in the path.** It looks for `[01xxxxxxxxxxxxxx]` (16 hex digits) in the file or folder
   name. An update ID (`...800`) maps to the base ID (`...000`). On this machine, 6 of 27 Switch
   entries carry a tag, and 3 of those have Eden play time.
2. **Learned from a launch.** On `game_started` with Eden, it snapshots `playtime.bin`. On
   `game_ended`, it diffs the file. If exactly one ID's seconds rose, it saves `path → id` to
   `mapping.json` in its data dir. Every game openSU launches in Eden is mapped after its first
   session.

A game that is neither tagged nor launched from openSU shows no play time. It is not guessed. Eden's
log has `Booting game: <id> | <title>`, but only for the last two sessions, so the plugin does not
read it.

### Merge with PlayHistory

`PlayHistory` records only when openSU launched a game. It measures no durations, so openSU adds no
seconds of its own. The rule lives in `library::PlayHistory::apply(games, reports)`:

- **Play time**: for a ROM game, `playtimeMinutes = Σ seconds / 60` over the play-time sources that
  report it. Each emulator counts only its own sessions (Eden's counter never includes a Ryujinx
  session), so the sum counts each session once.
- **Duplicate sources**: a game reported twice by the same source counts once, because reports
  are keyed by source id and game id.
- **Store games**: they keep the store's own value. v1 claims cannot name store games.
- **If openSU later measures sessions itself**: it must not add its measurement to a game that
  any source reports, because Eden's counter already includes sessions openSU launched.
- **Last played**: unchanged, the later of openSU's launch time and the store's value.

### The plugin (`plugins/eden-playtime/main.lua`)

```lua
-- Eden's play time: <data>/play_time/playtime.bin, records of { u64 program_id, u64 seconds } LE.
local RECORD = 16
local MAPPING = opensu.paths.data .. "/mapping.json"

local learned = {}
local before_session = nil

local function load_mapping()
  local text = opensu.fs.read(MAPPING)
  if text then
    learned = opensu.json.decode(text) or {}
  end
end

-- Program id (16 upper-case hex digits) to seconds; nil while Eden is mid-write.
local function read_database()
  for _, root in ipairs(opensu.paths.read_roots) do
    local file = root .. "/playtime.bin"
    local before = opensu.fs.stat(file)
    if before then
      local bytes, err = opensu.fs.read(file)
      if not bytes then
        opensu.log.warn("cannot read " .. file .. ": " .. err)
        return nil
      end
      local after = opensu.fs.stat(file)
      if not after or after.size ~= before.size or after.mtime ~= before.mtime
          or #bytes % RECORD ~= 0 then
        return nil
      end
      local database = {}
      for at = 1, #bytes, RECORD do
        local id, seconds = string.unpack("<I8I8", bytes, at)
        if id ~= 0 then
          database[string.format("%016X", id)] = seconds
        end
      end
      return database
    end
  end
  return {}
end

local function tagged_id(path)
  local hex = path:match("%[(01%x%x%x%x%x%x%x%x%x%x%x%x%x%x)%]")
  if not hex then
    return nil
  end
  hex = hex:upper()
  if hex:sub(-3) == "800" then
    hex = hex:sub(1, -4) .. "000"
  end
  return hex
end

local function program_id(game)
  return tagged_id(game.path) or learned[game.path]
end

opensu.register_playtime_source{
  claims = { source = "rom", systems = { "switch" } },
  read = function(games)
    local database = read_database()
    if not database then
      return nil
    end
    local reports = {}
    for _, game in ipairs(games) do
      local id = program_id(game)
      local seconds = id and database[id]
      if seconds and seconds > 0 then
        reports[game.id] = { seconds = seconds }
      end
    end
    return reports
  end,
}

opensu.on("game_started", function(game)
  if game.system == "switch" and game.emulator == "Eden" then
    before_session = read_database()
  end
end)

opensu.on("game_ended", function(game)
  local before = before_session
  before_session = nil
  if not before or game.emulator ~= "Eden" or program_id(game) then
    return
  end
  local after = read_database()
  if not after then
    return
  end
  local rose = {}
  for id, seconds in pairs(after) do
    if seconds > (before[id] or 0) then
      rose[#rose + 1] = id
    end
  end
  if #rose == 1 then
    learned[game.path] = rose[1]
    opensu.fs.write(MAPPING, opensu.json.encode(learned))
    opensu.log.info(game.title .. " is " .. rose[1])
  end
end)

load_mapping()
```

`load_mapping` reads a file, so it yields. The host therefore runs `main.lua` itself as a coroutine
after the registration calls are made. Registration stays synchronous.

## 7. Implementation plan

### Phase 1: runtime, play-time sources, Eden

This phase is about 2,000 lines of C++ plus 800 of tests. New target `opensu_plugin` in
`src/plugin/`. It depends on `library`, `net`, `fileio`, `config` and Lucent, and nothing below
`app` depends on it.

| Owner | Owns | ~Lines |
| --- | --- | --- |
| `cmake` / `src/plugin/CMakeLists.txt` | `opensu_lua` from `LUA_ROOT`, sources as `LANGUAGE CXX`, excluded from clang-tidy | 60 |
| `plugin/lua_runtime.*` | One sandboxed `lua_State`: allocator cap, count hook, opened libs, protected entry with traceback | 250 |
| `plugin/manifest.*` | `plugin.json` parsing, validation, path-token expansion through `config::Config` | 150 |
| `plugin/permissions.*` | Granted set; path and host checks (canonical prefix, HTTPS, per-hop) | 120 |
| `plugin/host_api.*` | The `opensu` table: log, json, fs, paths, http, titles, register/on | 400 |
| `plugin/plugin_worker.*` | Thread per plugin, coroutine scheduler, fault counting, disable | 300 |
| `plugin/io_pool.*` | Two threads running fs/http requests, completions back to workers | 120 |
| `plugin/plugin_host.*` | Discover bundled and user dirs, load enabled plugins, post events, `take()` reports and faults | 250 |
| `net/web_client` | A body cap and a redirect check callback (both new options; artwork can use the cap too) | 60 |
| `library/game.hpp`, `roms.cpp` | `Game::file` for ROMs (the ROM path Lua sees) | 10 |
| `library/play_history.*` | `apply(games, reports)`: the merge rule in section 6 | 40 |
| `settings/settings.*` | `plugins` key: enabled and granted per id | 80 |
| `app/settings_controller`, `ui/settings_page` | "Plugins" category: a toggle row per plugin, a fault note, the consent dialog on enable | 200 |
| `app/shell_app` | Compose `PluginHost`, post `game_started` and `game_ended` around the handoff, apply reports | 40 |
| `plugins/eden-playtime/` | The plugin above and its manifest | 120 |

Tests, all through the shipping code (`tests/plugin/`):

- **Runtime**: sandbox escape attempts must fail. These cover `load` of bytecode, `string.dump`,
  `io`, `os`, `debug`, `require("../x")` and `_ENV` tricks. The suite also covers the memory and
  instruction limits tripping, and a fault leaving the host alive with the plugin disabled after
  3 faults.
- **Permissions**: symlink and `..` escapes, an undeclared host, and a redirect to an undeclared
  host.
- **Host with fakes**: `PluginHost` is constructed with a fake `IoPool` backend (an in-memory file
  tree and canned HTTP answers). The real Lua runtime and API run against it.
- **Eden plugin**: the real `main.lua` runs through the host against fake trees. They cover a
  synthetic `playtime.bin`, a half-written file (returns `nil`, previous report kept), tag mapping
  including the `800` update ID, and learning from a started/ended pair with one and with two
  rising IDs.
- **Merge**: `PlayHistory::apply` with two sources on one game sums them, and a store game is
  untouched.
- **Reproduction**: `--hidden` with the control channel, reading the details page of
  `Donkey Kong Country Tropical Freeze [0100C1F0051B6000]` on this machine. Expected:
  "Play time 45 min".

Docs:

- A `docs/plugins.md` API reference for plugin authors.
- Codemap rows for `src/plugin/`.
- A new project-state capability.

### Phase 2: settings rows, game actions, artwork sources

API v2, about 800 lines plus tests. Candidate plugins: more emulators' play time (Ryujinx, PCSX2
and DuckStation keep play-time files; verify each format from its source first) and a SteamGridDB
artwork source with an API key setting.

### Phase 3: library sources, then Epic moves

API v3, about 1,200 lines plus tests. `Source` is a closed enum used by `label`, the launcher
badges, `ownedIn` icons, the source filter and the install jobs.

1. **Restructure (its own commit).** Add `Source::Plugin` plus `Game::origin` (the plugin id).
   Badges and filters become driven by a source descriptor rather than the enum.
2. **Add API v3.** Add `register_library_source` and a `run` permission that lists the exact
   program names a plugin may execute. Execution goes through `launch::runCaptured`, argv only,
   with no shell and a timeout.
3. **Move Epic.** In one commit, the `epic-library` bundled plugin replaces `epic::Provider`, and
   `epic.cpp` listing code and its tests are deleted. Its tests move to the plugin through the host
   with a fake `legendary`. Epic sign-in and install stay C++ and keep `Source::Epic`, with the
   listing provided by the plugin.

At no point do a C++ and a Lua version of the same integration coexist.

Epic goes first because it is self-contained, already slow (legendary), holds no secret in
openSU, and exercises every library-source feature (owned, installed, artwork URL, launch spec).
Steam, GOG, ROMs, artwork fetchers, launch and sign-in stay C++ for the reasons in section 2.

## 8. Cost, risks, alternatives

| Phase | Size | Calendar (one agent) |
| --- | --- | --- |
| 1 | ~2.8 k lines with tests | 3 to 5 days |
| 2 | ~1.2 k | 2 to 3 days |
| 3 | ~2 k including the `Source` restructure | 4 to 6 days |

Risks:

- **API churn.** v1 is deliberately tiny. Versions are integers, each a superset of the last, and
  the host supports one version at a time until outside plugins exist.
- **Security.** The sandbox holes that matter are path escapes (canonical prefix checks plus
  tests), redirects (checked per hop) and process execution (absent until v3, then argv-only and
  allow-listed). User plugins are off by default and need an explicit grant. The residual risk is
  denial of service: a C-side pattern stall wedges that plugin's worker, not the UI.
- **Performance.** A state costs about 200 KB. One worker thread per plugin plus a 2-thread I/O
  pool. Nothing runs per frame. Startup adds about 1 ms per plugin to load and compile `main.lua`.
- **Maintenance.** It is one more vendored dependency (Lua is about 30 C files, stable for years)
  and one more language in the repo.

Alternatives:

- **C++ plugin interface (dlopen).** There is no sandbox, a crash takes openSU down, and it needs
  a C ABI to avoid compiler lock-in. Plugins must be built per platform and architecture. It adds
  cost without the benefit (user-editable, safe extensions). Rejected.
- **No plugins: Eden play time in C++.** An `EmulatorPlayTime` owner in `library/` reading
  `playtime.bin` with the same mapping and merge rule is about 150 lines plus tests, roughly a day.
  It is a tenth of phase 1.

Recommendation: build the Lua host only if at least two or three more plugin-shaped integrations
are actually wanted soon. Examples are more emulators' play time, SteamGridDB art, and Heroic,
Lutris or itch libraries. If Eden play time is the only near-term want, implement it in C++ now
and keep this design for when a second extension appears. The mapping and merge rules in section 6
apply either way.
