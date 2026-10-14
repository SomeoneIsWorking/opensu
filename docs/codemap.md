# Code map

Where each concept lives. Read this before placing code; update it in the change that adds or
moves an owner. The UI replicates iiSU, so UI owners cite the iiSU function they reproduce and the
evidence is in `reference/iisu/` (the private `opensu-re` repository, checked out at the
gitignored `docs/reference/`).

## Entry and composition

| Path | Owns |
| --- | --- |
| `src/config/arguments.*` | The command line as typed values (`--render`, `--keyboard`, `--session`: openSU is the login session, `--hidden`: the shell with an unmapped window, no Gamescope session, no pads, free control port, no Steam client: `startsSteam()`) |
| `src/main.cpp` | Starts the nested session or the shell; `--render FILE` renders one frame headless |
| `src/app/shell_app.*` | Composition: catalog, controller reader, Steam client, launches, the drawn shell, frame loop; plays the UI sounds at the input events that iiSU plays them at; streams artwork every loop pass, drawn frame or not |
| `src/app/shell_window.*` | The shell's SDL window on gpu2d (`GpuWindow` and `Canvas`): resizable, translucent, 960x600 minimum, unmapped without vsync for `--hidden` and `--render`; logs the device, driver and translucency; the frame's keys, typed text, pointer and resize from SDL events; a frame presented at most 60 a second, or drawn offscreen and read back; the monitor's size and refresh and filling it for the Gamescope overlay |
| `src/app/sdl_keys.*` | SDL scancodes onto the shell's key codes (`input::keys`) |
| `src/app/control_bridge.*` | The hand-over between the control channel's threads and the loop: queued input, reload and close requests, the published `ShellSnapshot`, the frame handshake |
| `src/app/game_screen.*` | The window's relation to a running game: the Gamescope overlay and game-time key watcher, or the hidden window flag; shows the window only while a menu or page is up |
| `src/app/guide_menu_controller.*` | The Guide menu's buttons and what each entry does (sections, Devices, Settings, close game, quit, the power button and list: `host::Power` with a second press for restart and shut down, `host::SessionMode` for Switch to session mode (second press), Install session mode (opens the dialog) and, in a login session, Switch to desktop) |
| `src/app/session_install_controller.*` | The Install session mode flow: the dialog, then the password on the on-screen or physical keyboard, the root step on a worker, a wrong password asking again, the password wiped once sudo has had it |
| `src/app/keyboard_walk.*` | A pad button on the on-screen keyboard for the entries that type a line (folder path, password): the one implementation `path_editor` and the session install share |
| `src/app/quick_menu_controller.*` | The quick menu's rows and buttons: volume, mute, output, brightness, Bluetooth, controller batteries, Devices, Save state, Load state and Reset game for a game on `opensu-retro` (Load and Reset on a second press), Close game |
| `src/app/retro_session.*` | The shell's side of `opensu-retro`'s control channel: each launch's `OPENSU_RETRO_CONTROL` file under the runtime dir, the port read from it, `/status`, save, load and reset over `lucent::http` loopback, the load failure a runner leaves handed over and the file removed |
| `src/app/game_end.*` | The toast for a launch that is over: a runner's load failure, `<title> crashed (SIGSEGV)` for a death by a signal the player did not send, the launch's failure, or `<title> closed` |
| `src/app/devices_controller.*` | The Devices page: Bluetooth, Controllers, Audio output and Display tabs and what their rows do |
| `src/app/host_services.*` | Power, session mode, Bluetooth and backlight backends for this machine; the one place a hidden run is made read-only |
| `src/app/bluetooth_session.*` | BlueZ reads off the loop and one change at a time, with notices; pairing is pair, trust, connect |
| `src/app/controller_roster.*` | The pads connected now in player order, with battery and held buttons (the live button test) |
| `src/app/audio_outputs.*` | The sound outputs and the default one, through `SystemVolume` |
| `src/app/brightness_control.*` | Display brightness through the backlight, absent without one |
| `src/host/` | System services over `busctl`, `systemctl` and `bluetoothctl` behind fakeable seams: `runner` (run or hold a program), `bluetooth` (BlueZ), `power` (logind: sleep, restart, shut down), `session_mode` (install session mode, switch to it and back through the selector, `sudo` and the logout call; see Session and processes), `backlight` (sysfs read, logind write), `secret.hpp` (wiping a password buffer) |
| `src/app/layout_picker.*` | START: the options panel's buttons (layout cards, icon size, pin, sort, source, installed, hidden, search row) and saving what is chosen |
| `src/app/preferences.*` | The loaded `settings::Settings` and saving them, with a toast when the file cannot be written |
| `src/app/search_controller.*` | The search panel's pad buttons and keyboard text; the typed text is the view's search |
| `src/app/context_menu_controller.*` | Opening the focused tile's menu and what its entries do (launch, details, hide/unhide, open, refresh, sign in) |
| `src/app/details_controller.*` | The details page's buttons: open for the focused game, Play or Install, the emulator pick, Download <emulator> (start or cancel the emulator download), Hide, Back; keeps the page current as the library changes |
| `src/app/breadcrumb_trail.*` | The one computation of the breadcrumb trail from the shell's state (section, folder, search, filters, open game) |
| `src/app/settings_controller.*` | The Settings screen's controller: rows per category, input routing, saving through `Preferences`, folder chooser and path entry; Install folders shows each store's resolved folder (override, default or the store's own) |
| `src/app/plugin_service.*` | Which plugins run (bundled unless turned off, user plugins only when turned on with their exact permissions granted), refresh requests, play-time reports applied to the library, the stop toast |
| `src/app/plugin_settings.*` | Settings › Plugins: a toggle and an access row per plugin, the consent dialog before a user plugin runs |
| `src/app/emulator_downloads.*` | The emulator download job for official AppImages and built-in emulators' cores: one download at a time on a thread with progress and cancel, the update check (shows, never installs; the buildbot listing read once for every core), removal of what openSU downloaded; news handed to the loop in `service` |
| `src/app/emulator_settings.*` | Settings › Emulators: each emulator's install state, source and version (a core's: libretro buildbot and its build date, or the folder it was found in) with Download / Update / Remove (two presses), Check for updates, the emulator per system |
| `src/app/steam_library_settings.*` | Settings › Steam libraries: the libraries with path, free space, games and size; add, make default, remove an empty one, on a worker thread; read-only from `libraryfolders.vdf` while Steam is not running |
| `src/app/path_editor.*` | Choosing a folder on the chooser or typing a path; one request at a time, refusals shown with their reason |
| `src/app/artwork_delivery.*` | Hands saved artwork, store contents and UI sounds to the shell and sound player |
| `src/app/volume_control.*` | The volume actions onto `SystemVolume` and its backend; polling; the OSD listener |
| `src/app/shortcut_editor.*` | The remap capture: pick an action, press the new combo or pad chord, refusals |
| `src/app/shortcut_router.*` | Keys, pad events and game keys onto shell events and actions through `Shortcuts`; the quick menu and quit actions are done here |
| `src/app/frame_png.*` | The drawn shell as PNG bytes, for the control channel's frame and `--render` |
| `src/app/panel_flow.*` | The launch/install panel flow taken out of `shell_app.cpp` |
| `src/app/pointer_router.*` | The mouse onto the shell's actions: hover focus on a moved pointer, left click = focus + A (or the dock, page, breadcrumb and panel-button action), right click = the target's context menu (never B), wheel = a pad step; asks `PointerHost` (`ShellApp`) what is under the pointer |
| `src/app/control_channel.*` | Loopback HTTP control channel (`lucent::http::Server`): shell state, input, frames, `/signin/<store>[/start]` |
| `src/app/sign_in.*` | Store sign-in steps: open the sign-in page in the default browser, finish with the code (GOG token, Epic via `legendary auth --code`) |
| `src/app/launcher_status.*` | The launcher badges' states from the Steam client, its downloads and the catalog's store statuses |
| `src/app/install_job.*` | The store-neutral install job: its thread, the latest report the loop takes, the licence answer |
| `src/app/cli_install_job.*` | An install by a downloader program: run it, turn its logged progress into reports, fail with its reason; the base of the Epic and GOG jobs |
| `src/app/steam_install_job.*`, `epic_install_job.*`, `gog_install_job.*` | One Steam install (walks Steam's installer, follows its queue); one Epic install (`legendary install`); one GOG install (`gogdl download`: token handed over and taken back, Linux or Windows build, the install recorded) |
| `src/app/installs.*` | The installers by store, one install at a time; which stores install; each store's own install folder (`ownFolders`: GOG under the data dir, legendary's, Steam's default library) |
| `src/config/config.*` | The one reader of the environment, into typed immutable config (cache, data and config dirs included, the XDG bases plugin paths expand from, legendary's config dir, the logind session id); where the Gamescope fork binary is (`gamescopeBeside`, relative to `/proc/self/exe`) and the libretro runner beside the executable (`retroRunnerBeside`); the runner's saves and system folders (`savesDir`, `retroSystemDir`); the runtime dir (`runtimeDir`) and the runner's control file (`retroControl`, from `OPENSU_RETRO_CONTROL`) |
| `src/settings/settings.*` | The player's saved preferences (Library layout, dock pin, icon size, sort and filters, hidden games, last-played times, Settings-screen choices, interface scale, install folders, shortcut remaps, plugins turned on or off with the permissions granted) as JSON under the config dir; defaults on a missing or corrupt file |
| `src/settings/install_folders.*` | The one resolver of where a store installs: a store's override, else the default, else none (the store's own choice); `resolve` adds the store's own folder and says which of the three applies; validation (exists, writable) |
| `src/fileio/atomic_write.*` | Whole-file writes through a `.part` file and a rename |
| `src/zip/zip_archive.*` | The one zip reader: end record, central directory, local header offset, checked extraction, an in-memory archive, and the CRC-32 (`crc32Of`) zips and libretro listings are checked with; used by artwork and core downloads |
| `extension/opensu-signin/`, `extension/CMakeLists.txt` | Firefox/Zen WebExtension that hands a GOG or Epic sign-in code to the control channel; packed into `opensu-signin.xpi` and installed beside the assets |

## Session and processes (G003, G004)

| Path | Owns |
| --- | --- |
| `src/session/compositor_session.*` | Re-running openSU inside its own Gamescope when not already in one: nested at the monitor's size on a desktop, top level on DRM at the native mode for `--session`; starting a fresh Gamescope after an abnormal end nobody asked for |
| `src/session/restart_budget.*` | How often a crashed compositor is started again (3 restarts in any 2 minutes) |
| `src/host/session_mode.*` | Session mode, the one owner of switching sessions: `installed()` (an entry for this openSU in a directory SDDM reads, and the selector), the root step (`sudo -S -k` with the password on stdin, one `install-session.sh`), Switch to session mode (selector `opensu`, then KDE's `org.kde.Shutdown.logout`) and Switch to desktop (selector `restore`, then stopping the Gamescope scope); read-only wrapper for hidden runs |
| `src/host/login_session_end.*`, `src/session/login_session.*` | What the next login is when the login session ends: an abnormal end (Gamescope non-zero, by a signal, or within 30 s) runs `sudo -n <selector> restore` so SDDM autologin cannot relogin in a loop; a logout or shutdown signal and a deliberate Switch to desktop (the note) leave it alone; `LoginSession` composes the top-level `CompositorSession` with the judgement and the desktop fallback |
| `src/host/desktop_request.*` | The note Switch to desktop leaves in the runtime dir (it marks the end deliberate), and, as the fallback when the selector is not installed, `startplasma-wayland` run in the login session's place once Gamescope has ended |
| `packaging/install-session.sh`, `packaging/opensu-session-select.sh` | The two root-run POSIX sh scripts, installed to `<prefix>/libexec/opensu/`: the one-sudo installer (session entry, `/usr/local/libexec/opensu-session-select`, `/etc/sudoers.d/zz-opensu-session-select`) and the selector (`opensu` or `restore`: SDDM drop-in `zz-opensu-session.conf` or the last session in `state.conf`) |
| `src/session/gamescope.*` | The Gamescope command line, nested (`--close-focused-window` included) or top level (`--backend drm`) from `Output::native()` |
| `packaging/opensu-session.desktop.in` | The Wayland session entry, installed to `<prefix>/share/wayland-sessions/opensu.desktop` (`install-session.sh` copies it where SDDM reads) |
| `cmake/Gamescope.cmake` | Building the pinned Gamescope fork in podman, or natively in an environment that already has its dependencies (`OPENSU_GAMESCOPE_BUILD_ENV`: CI's Fedora container, the AppImage container), with `OPENSU_GAMESCOPE_MESON_OPTIONS` for a distribution lacking some (commit, build steps, staging and install path); `OPENSU_BUILD_GAMESCOPE`. `cmake/GamescopeFetchPin.cmake` fetches a new pin into an existing clone, `cmake/GamescopeImage.cmake` builds the image if absent, `cmake/GamescopeLddCheck.cmake` checks the staged binary's libraries |
| `packaging/gamescope-build/Containerfile` | The Gamescope build image: host's Fedora release plus Gamescope's build dependencies |
| `src/session/monitor.*` | The output's size and refresh, from SDL's display modes (X11 video): the primary monitor before the window opens, the window's display after |
| `src/session/gamescope_overlay.*` | openSU's window as Gamescope's overlay over a running game |
| `src/session/game_keys.*` | The shortcuts that work while a game has the keyboard (Guide, volume), matched against the `Shortcuts` table from XInput2 raw keys |
| `src/session/gamescope_windows.*` | Which processes own a window Gamescope would show; tells the handoff when a game is on screen |
| `src/launch/instance.*` | One transient systemd user scope per launch, its program started with no signal blocked; stopping it ends the whole tree; how the started process ended (`exitStatus`) |
| `src/launch/handoff.*` | Starting a game, reporting its progress until it shows a window, hiding the shell until it ends; how a game in an Instance ended (`Hooks::ended`), unless force-closed |
| `src/launch/process_tree.*` | Finding processes by command line, ending trees |
| `src/launch/command.*`, `argv.hpp` | Short-lived children, a child streamed line by line, its stdout captured or fed one line on stdin (`runCapturedWithLine`, the sudo password) (own `opensu_command` target, so `library` can run `legendary`), executable lookup, exec argv |
| `src/launch/steam_gate.hpp` | What a Steam launch waits for from the owned Steam client |
| `src/launch/launch_progress.*` | A launch's stage before its window (waiting for Steam, updating, starting, loading) and its line of text |
| `src/launch/game_windows.hpp` | What the handoff asks the display: does the game show a window yet |
| `src/steam/client.*` | The Steam client openSU owns: start in background, readiness, state |
| `src/steam/desktop_steam.*` | Detecting a Steam client running outside openSU |
| `src/steam/orphaned_steam.*` | Finding and ending a Steam scope left by an openSU that died |
| `src/steam/devtools.*` | Running JavaScript in Steam's SharedJSContext over Chrome DevTools |
| `src/steam/libraries.*` | Steam's library folders through `SteamClient.InstallFolder`: read, add, set default, remove (refused for Steam's own library or one that holds games) |
| `src/steam/downloads.*` | Steam's live download queue: parsing and reading it |
| `src/steam/launch_activity.*` | Steam's game actions and running state per app, recorded in its SharedJSContext |
| `src/steam/install_wizard.*` | Installing a Steam app through Steam's own installer, licence steps to the player |

## Library

| Path | Owns |
| --- | --- |
| `src/library/game.*` | The launchable `Game` record |
| `src/library/catalog.*` | Building the catalog's providers from all sources (`game.*` holds `readProvider` and `assemble`, the merge) |
| `src/library/catalog_loader.*` | Listing every store off the frame thread: one thread per provider, listings handed over as they arrive, `Loading` until a store's first one |
| `src/library/steam.*`, `epic.*`, `gog.*`, `roms.*` | One source each |
| `src/library/gog_auth.*`, `gog_token.*` | GOG's OAuth sign-in and the saved token (`<data dir>/gog-token.json`); `writeOwnerOnly`, the one owner-only atomic write |
| `src/library/gogdl_auth.*` | The token as gogdl's `--auth-config-path` file, written for an install and read back |
| `src/library/gog_installs.*` | Where GOG files live under the data dir (`Paths`) and the record of finished GOG installs |
| `src/library/install_log.*` | What legendary and gogdl log while installing: progress fraction, ERROR/CRITICAL lines |
| `src/library/sections.*` | The dock's sections (Home, Library), the active one and L1/R1 cycling with wrap; Library's layout modes and their keys |
| `src/library/shelf.*` | What the grid holds: Home's installed store games (`homeShelf`); Library's launchers, All games and consoles (`libraryShelf`); a console's ROMs, a launcher's library, the combined library; moving between them and between sections |
| `src/library/library_query.*` | The one owner of what the library shows: the search ranking, the filters (installed, source, hidden), the sort, `HiddenGames`; Home, Library, folders and All games take their games from `visibleShelf` |
| `src/library/text_fold.*` | Case, accent and white-space folding titles are compared in |
| `src/library/emulator_choice.*` | The emulator picked per ROM and per system (`settings.json`): points the game's `launch` and `emulator` at the game's pick, else the system's; `unavailable` when it is not installed or cannot start |
| `src/library/play_history.*` | When openSU last launched each game, for the recent sort of stores that do not say |
| `src/library/play_time.*` | `PlayTimeReports`: each play-time source's latest report, summed into ROM games' play time |
| `src/library/emulator_game_list.*` | An emulator's own game list (its id, title, path, metadata per game) matched to openSU's games by path, then by unique title key |
| `src/library/titles.*` | The same title across stores: the comparison key, merged copies, preference order |
| `src/library/rom_titles.*` | A ROM's display title from its name: tags, release numbers and the ", The" order (`cleanTitle`); the one title cleanup, also the release-number rule the libretro matcher uses |
| `src/library/param_sfo.*` | The PlayStation PARAM.SFO reader (`sfoStrings`): its UTF-8 values by key |
| `src/library/dump_info.*` | What a game folder says about itself (`readDumpInfo`): a PS4 dump's `sce_sys/param.sfo` title and `icon0.png` |
| `src/library/arcade_names.*` | Arcade short name to description: the libretro-database `.dat` parser and `NameDb`, the parsed names kept under `<cache>/names/` |
| `src/library/rom_systems.*` | Known systems: folder names, game files, the file a game folder starts; the tables are `constexpr` (`name_list.hpp`) |
| `src/library/emulators.*` | Which emulator runs each system here, and its command line, from the catalog: openSU's downloaded AppImage, else a package on `PATH`, an AppImage in the usual folders, a Flatpak; a built-in emulator when its core is found and `opensu-retro` is there (`opensu-retro --core <core> <rom>`); every emulator of a system, installed or not (`options`), which a ROM's details page lists |
| `src/library/emulator_catalog.*`, `assets/emulators.json` | The emulator catalog: id, name, systems, settings folder, launchers (programs, AppImage name, Flatpak id, arguments) and, for emulators with an official AppImage, the release API, asset pattern and download host; a built-in emulator names its libretro core (`"core": "mgba_libretro"`) instead of launchers |
| `src/library/libretro_cores.*` | Built-in emulators' cores: the lookup (openSU's `<data>/opensu/cores/`, then RetroArch's and the system's core folders), the buildbot's `.index-extended` parsed, a core zip downloaded on the buildbot's origin only, its one `.so` taken out, checked against the listed CRC-32 (the library's, not the zip's) and written whole, its `<core>.json` record (build date, CRC-32), removal |
| `src/library/emulator_release.*` | An emulator's latest official AppImage from its GitHub or Forgejo release JSON, downloaded beside the one in use, checked against the published SHA-256 (or recorded as seen, HTTPS from the project's host only), made executable and switched to |
| `src/library/downloaded_emulators.*` | The AppImages openSU downloaded under `<data>/opensu/emulators/<id>/<asset id>/`: the `installed.json` record (the atomic switch), old versions cleared, removal |
| `src/library/appimage_runtime.*` | Whether an AppImage can run here: its runtime's libfuse2 need, `/dev/fuse`, `fusermount`; the refusal names the install command for the distribution |
| `src/artwork/libretro_index.*` | Matching a ROM's name to libretro-thumbnails' box art listing |
| `src/artwork/arcade_dat.*` | The pinned libretro-database arcade listings (FinalBurn Neo, MAME 2016): download against size and CRC-32, parse, merge; the fetcher keeps the result in `NameDb` |
| `src/artwork/artwork_store.*` | Downloaded artwork on disk under the cache dir: paths, misses, listings, frame glyphs, the starter pack file, UI sounds (`sound/<file>`, WAV or OGG) and the dock's icons (`nav/`) |
| `src/artwork/artwork_fetcher.*` | The background downloads (queue stages per game, on-screen games first, a settled game reported as not saved): Steam's CDN for Steam, gamesdb (else the library tile) for GOG, the key image URL for Epic, libretro-thumbnails for ROMs, iiSU's starter pack for console cards, iiSU's border pack for frame glyphs, the APK's sounds and nav drawables (`ApkAsset`) |
| `src/artwork/apk_archive.*` | iiSU's release APK read by HTTP ranges: central directory once, any entry by range with its CRC checked; `fetch` is find and extract with a found/missing/failed result (glyphs, sounds and nav icons use it) |
| `src/artwork/starter_pack.*` | iiSU's starter pack: its entry taken out of the APK against a pin, and a system's card (WebP, decoded and re-encoded by `gpu2d::image`) as PNG |
| `src/artwork/console_glyphs.*` | iiSU's frame glyphs: `border_pack.json` read once, a system's `logo_*.png` out of the APK |
| `src/artwork/iisu_assets.*` | Which APK entry is each UI sound and each dock icon (the nav table is valid for the pinned APK only) |
| `src/audio/effect.*`, `debounce.*` | iiSU's UI sounds openSU plays (`yp8`): effect to APK file name, the Domino cues and `dominoFor(tileCount)` (`xp8.a`), the 91 ms repeat rule for Enter/ExitConsolesApps and the Domino cues; pure (`opensu_audio_model`) |
| `src/audio/volume_backend.*` | PC output volume through wpctl or pactl (injectable runner), a read-only shadow for hidden runs, the install advice when no backend exists |
| `src/audio/system_volume.*` | Volume and mute state, steps, one listener, async polling of changes made elsewhere |
| `src/audio/sound_clip.*` | A UI sound decoded to PCM without a device: WAV through `SDL_LoadWAV`, OGG through libvorbisfile |
| `src/audio/sdl_sound_player.*` | The shell's UI sounds on SDL audio (`opensu_audio`): clips decoded on `load` with or without a device, the default device opened once by `open` (never in hidden runs or tests), one bound stream per effect, `play` restarts the clip through the debounce |
| `src/net/web_client.*` | HTTPS GETs over libcurl, with headers, and streamed file downloads with progress and cancel; shared by artwork, the stores and emulator downloads; `request` with a body cap, and `request` and `download` with each redirect hop checked (`Refusal`) |
| `src/vdf/` | Valve KeyValues parser |
| `src/device/battery.*` | Battery level and charging state from sysfs |
| `src/gamepad/event.*` | The shell's controls (`Button`, `Event`) (`opensu_pad`) |
| `src/gamepad/pad_translator.*` | One physical pad's evdev events as the virtual pad's and as controls |
| `src/gamepad/evdev_device.*` | An evdev node: capabilities, grab, state, reads; whether a reader takes opensu's own virtual pads (`OwnVirtualPads`: the shell skips them, `opensu-retro` reads them) |
| `src/gamepad/virtual_pad.*` | The uinput Xbox 360 pad a game reads |
| `src/gamepad/pads.*` | Every pad, read always and hot-plugged; holds them during a game and blocks them while a menu or page is up; lists the connected pads (`connected()`), and every event names its pad (`Event::source`) |
| `src/gamepad/direction_repeat.*` | Held-direction repeat for pads and keys |
| `src/input/shortcuts.*` | The one chord table: every `Action` (pad buttons, Guide, volume, Quit) with its default key combos and pad chords, the player's overrides (`rebind`, with conflict refusals), labels and settings spellings |
| `src/input/key_names.*` | The shell's key codes (`input::keys`, GLFW's numbering, letters as ASCII) and their names: `Combo`, modifiers, bindable keys, X11 key names for the in-game watcher |
| `src/input/pad_chords.*` | Pad chords (L2, R2, L3, R3 or Guide plus a button): the held modifier swallows its trigger and emits the action; Guide alone is a tap that comes out on release |
| `src/input/prompts.hpp` | `Prompts`: the last device and the shortcuts, so key caps in hints follow remaps |
| `src/input/keyboard_bindings.*` | `KeyFrame` (one frame's key downs, ups, repeats and typed text), `KeySource`, the frame's pressed `Combo`, and typed text; the keys themselves come from `Shortcuts` |
| `src/input/last_device.*` | Which device (pad, or keyboard and mouse) gave the latest real input; read by the prompt painters |

## Built-in libretro runner (`opensu-retro`)

Design: `docs/design/libretro.md`. `opensu_retro` and the `opensu-retro` executable, built beside
`opensu`.

| Path | Owns |
| --- | --- |
| `cmake/Libretro.cmake` | `libretro.h` from libretro-common at a pinned commit, SHA-256 checked (`opensu::libretro_header`) |
| `src/retro/main.cpp` | The command line; the default saves (`Config::savesDir`) and system (`Config::retroSystemDir`) folders |
| `src/retro/runner_app.*` | Composition and the frame loop: pacing on a clock at the core's rate, input, sound, save checks, control commands between frames, `--hidden` |
| `src/retro/control_server.*` | The runner's loopback control channel: `{"port", "pid"}` (or `{"error"}` on a load failure) in the `OPENSU_RETRO_CONTROL` file, `/status`, `/state/save`, `/state/load`, `/reset`, each POST run on the frame loop |
| `src/retro/save_state.*` | The state slot `<saves>/<rom stem>.state`: serialize, atomic write, unserialize |
| `src/retro/core.*` | One core: `dlopen`, `retro_*` symbols, init/load/run/unload, the context-free callback trampolines (one file-scope pointer) |
| `src/retro/core_environment.*` | The environment commands answered; everything else refused |
| `src/retro/core_options.*` | Core option definitions (v0, v1, v2, intl) and their default values |
| `src/retro/video_frame.*` | 0RGB1555, XRGB8888, RGB565 to RGBA; duplicate frames |
| `src/retro/audio_sink.*` | Resampling ring with ±0.5 % dynamic rate control |
| `src/retro/audio_output.*` | The sink on the SDL playback device, or drained by the clock when hidden |
| `src/retro/joypad.*` | `gamepad::Button` and SDL scancodes to `RETRO_DEVICE_JOYPAD` |
| `src/retro/save_memory.*` | Save RAM: load, change check, atomic `.srm` write |
| `src/retro/presenter.*` | The SDL window and its events; Vulkan swapchain or hidden offscreen image, frame texture, letterboxed draw, read-back |
| `src/retro/vulkan_device.*` | Vulkan instance (optional validation), device and graphics queue |
| `src/retro/shaders/` | GLSL for the frame, compiled by glslc into SPIR-V headers in the build tree (gpu2d's `gpu2d_compile_shaders`) |
| `tests/retro/` | Fake cores (`fake_core.cpp`, built three times: serializing, crashing, stateless), unit tests of each owner, `core_test`, and `runner_test` running `opensu-retro --hidden` end to end, its control channel included |

## Plugins

Design and API: `docs/design/lua-plugins.md`.

| Path | Owns |
| --- | --- |
| `cmake/Lua.cmake` | `opensu_lua`: Lua 5.4.9 fetched from lua.org by SHA-256, compiled as C++, only the libraries the sandbox opens |
| `src/plugin/manifest.*` | `plugin.json`: parsing, validation, path tokens, the permission list |
| `src/plugin/permissions.*` | A plugin's read paths and HTTP hosts, and the checks against them |
| `src/plugin/lua_runtime.*` | One sandboxed `lua_State`: memory cap, instruction and run-time limits, protected calls, coroutine activations that wait on I/O |
| `src/plugin/lua_json.*` | JSON to and from Lua values |
| `src/plugin/sources.*` | Source claims and the Lua shape of games, game-list entries and play-time reports |
| `src/plugin/host_api.*` | The `opensu` table (API v1) |
| `src/plugin/io_pool.*` | The two I/O threads plugins' file reads and HTTP run on |
| `src/plugin/plugin_worker.*` | One plugin's thread: load, refresh (game list, match, play time), fault counting |
| `src/plugin/plugin_host.*` | Discovery of bundled and user plugins, start and stop, results taken by the loop |
| `assets/plugins/` | Bundled plugins (`eden-playtime`: Eden's play time and game list) |

## Rendering and images

Design: `docs/design/sdl3-gpu-shell.md`. The 2D layer and image codecs are the gpu2d library, its
own repository (`GPU2D_ROOT`, default `~/repo/gpu2d`; pinned in `tools/check/pins.py` and fetched by
`tools/check/deps.py` like Lucent), with its own codemap and tests. The shell draws, decodes and
lays out text through it; the window, input and sound are SDL3's.

| Target | Owns |
| --- | --- |
| `gpu2d::image` | Decoding PNG, JPEG and WebP into RGBA, cubic resizing, PNG encoding to memory; no GPU. Used by the shell's covers and `/frame.png`, `opensu-retro` (`--frame-png`) and the starter pack |
| `gpu2d::render` | The SDL window and Vulkan device, the 2D canvas, text through SDL3_ttf, offscreen targets and the backdrop blur; the shell's renderer |

## Home UI (G002)

Pure model, unit-tested without a GPU (`opensu_grid`, `opensu_hud_model`):

| Path | Owns |
| --- | --- |
| `src/ui/home_layout.*` | Grid geometry; `visibleSlots`, the one visible-window rule painting, hit-testing and artwork requests share for Standard and WiiSu: cells, gaps, insets, placeholder slots, scrolling, page pill and page arrow rects, and the slot or page control under a point (`hx2.g`, `zj2`, `ou4.q`, `ys8.h/k/l`) |
| `src/ui/grid_focus.*` | D-pad focus movement and page crossing (`hx2.z/O`) |
| `src/ui/tile_motion.*` | Focus scale, domino entrance, press pulse, ring rotation, FastOutSlowIn, the rail's visual-index easing |
| `src/ui/section_view.*` | Per section: the grid viewport (3 rows x 4 columns, column-major, horizontal paging, both sections) and the presentation (Grid, XMB, Carousel) |
| `src/ui/rail_layout.*` | XMB and Carousel geometry (navigation.md 5.3): rectangles of the tiles near the canvas only (`first()`), from the fractional focus, left column, header card, marker and title anchors; the tile under a point (`railTileAt`) |
| `src/ui/rail_painter.*` | XMB/Carousel chrome: the recoloured section icon, the header card, the markers and the shadowed title |
| `src/ui/icon_recolour.*` | Reads a console card's border colours and recolours the section icon with them |
| `src/ui/dock_metrics.*` | The dock capsule's sizes and rectangles in dp (`gh3.i1/j1`, `jj2`); which item a pointer is on (`dockItemAt`) |
| `src/ui/dock_motion.*` | The dock's show/hide (`dockPinned`: Home always, Library by the pin option; else 1200 ms after L1/R1 on Library; show 170 ms ease-out from below, hide 125 ms ease-in straight down) and the icon pop |
| `src/ui/mode_chooser.*` | The options panel's state, focus row and scrolling layout (cards, icon size slider, pin, sort, source, filters, search); the card, row or slider level under a point |
| `src/ui/icon_size.*` | iiSU's icon size levels 1 to 20 and their scale formulas; shared by `home_layout` and `rail_layout` |
| `src/ui/search_panel.*` | Global Search state: field, drawn keyboard walk, results list, layout and hit-tests |
| `src/ui/details_page.*` | The details page: what it says about a game (`detailsFor`, `lastPlayedText`), its buttons, geometry, focus |
| `src/ui/breadcrumbs.*` | The breadcrumb trail's levels and geometry: cells, chevrons, fitting to the room, the level under a point |
| `src/ui/context_menu.*` | The tile context menu: entries per tile (`contextItemsFor`), focus, layout and hit-tests |
| `src/ui/panel_fade.*` | The fade and scale a panel shows and hides with |
| `src/ui/tile_geometry.*` | One tile's rectangles and radii (and `outerForContent`, the inverse of the frame inset); where a game tile's store icons sit |
| `src/ui/top_bar_metrics.*` | Top bar sizes in dp (`is7`, `hs7`, `dl3`) |
| `src/ui/top_bar_layout.*` | The status pill and the launcher badge cells in it, in pixels; the launcher under a point. The Hud paints and hit-tests from it |
| `src/ui/corner_hints.*` | Which prompts the bottom corners name, from what each button does now (`HintContext`) |
| `src/ui/clock_text.*`, `battery_icon.*` | Clock string and battery drawable choice |
| `src/ui/guide_menu.*` | The Guide menu's entries per context (home or running game), the power button below the rows, the power list (Switch to session mode or Install session mode outside the login session, Switch to desktop inside it), the armed entry, focus; its panel, row and power button rectangles |
| `src/ui/session_dialog.*` | The consent dialog model: title, paragraphs, Accept and Cancel focus, the progress line while a change runs; its layout and hit-test |
| `src/ui/quick_menu.*` | The quick menu's rows (the Settings rows), focus, panel layout and hit-tests |
| `src/ui/panel_frame.hpp` | The frame and chrome heights the side menus are laid out in |
| `src/ui/page_panel.*` | A settings-style page with its fade, as the Settings screen and Devices page use it |
| `src/ui/tile.hpp`, `tile_artwork.*` | The tile and when it shows the loading placeholder; the artwork decode, tickets, resident textures and pending count (`/state` `artworkPending`) of the shelf |
| `src/ui/launch_panel.*` | The launch/install card's state; its card and hint rectangles from measured widths, and the hint under a point |
| `src/ui/pointer_target.hpp` | What a pointer can be on (session dialog button, breadcrumb level, details button, dock item, tile, page control, layout card, options row, icon size slider, search key or result, context item or backdrop, Guide entry, Guide power button, quick row or slider, page row or slider, panel button, launcher badge) |

Painters and composition (`opensu_ui`):

| Path | Owns |
| --- | --- |
| `src/ui/shell.*` | The home screen: tiles, focus, paging, draw order; the scene into its own target so the dock glass blurs what lies behind it (`Canvas::blurBehind`, 8 dp) |
| `src/ui/hud.*` | Chrome around the grid: ground, top bar, corner hints, toast; grid insets |
| `src/ui/settings_page.*`, `settings_page_painter.*` | The Settings screen: categories, rows (toggle, choice, slider, path, action, info), focus, layout and hit-tests; its painter |
| `src/ui/settings_panels.*` | The Settings screen and what opens over it (folder chooser, on-screen keyboard): their state, fades and painters as one object |
| `src/ui/folder_chooser.*`, `folder_chooser_painter.*` | The console-friendly folder chooser: model and painter |
| `src/ui/volume_osd.*`, `volume_osd_painter.*` | The volume overlay: model (hold, fade) and painter |
| `src/ui/status_pill.*`, `glass.*` | The one status pill painter: clock, battery and the launchers inline, on a glass body |
| `src/ui/launcher_badges.*` | Launcher logos with status dots, the starting spinner and the download ring, drawn by the status pill painter; the hover light |
| `src/ui/breadcrumb_painter.*`, `details_page_painter.*` | The breadcrumb pill; the details page (cover, backdrop, title, badge, rows, buttons) |
| `src/ui/vector_icon.*` | The shipped SVG icons (`assets/icons/`), rasterised at drawn size |
| `src/ui/progress_spinner.*` | Material's indeterminate circular spinner (a starting launcher's ring) |
| `src/ui/button_glyph.*` | Controller button glyphs (`input_glyph_*`, LB/RB included), or the bound key's cap when keyboard and mouse were last used |
| `src/ui/dock_painter.*` | The dock capsule: glass, nav icons, LB/RB badges |
| `src/ui/mode_chooser_painter.*` | The options panel: the cards after iiSU's chooser, with sketched previews, then the slider and switch rows |
| `src/ui/search_panel_painter.*`, `context_menu_painter.*` | The search panel with its keyboard and the tile context menu |
| `src/ui/guide_menu_painter.*`, `quick_menu_painter.*` | The Guide menu on the left and the quick menu on the right, over the home screen or a running game |
| `src/ui/guide_panels.*` | The Guide and quick menus with their painters and the one hit test |
| `src/ui/session_dialog_painter.*`, `session_panels.*` | The dialog's painter (wrapped paragraphs, pill buttons with pad glyphs); the dialog and the password keyboard (a secret `SearchPanel`: dots, Caps, wiped on close) with their fades and one hit test |
| `src/ui/tile_painter.*` | One tile: shadow, ring, chrome, art (a spinner while it is on its way), platform frame, store icons; the name cards of a console, a launcher and All games |
| `src/ui/page_pill.*`, `page_arrow.*` | WiiSu page dots and page arrows |
| `src/ui/paint.hpp` | What the painters draw with: gpu2d's `Canvas`, `Colour` and `Point`, the layout's `Rect` on the canvas, corner radii, and the `ScopedClip` and `PanelScope` (scale about a point) scopes |
| `src/ui/round_shape.*` | Tessellated rounded shapes with per-vertex colour |
| `src/ui/platform.*`, `platform_stroke.hpp` | Console border sprites, logos, stroke colours |
| `src/ui/image_decoder.*` | Cover decode and downscale on a worker thread; the frame only uploads what it takes |
| `src/ui/glyph_textures.*` | The console glyphs drawn in a ROM tile's frame tab: files by system, textures on load |
| `src/ui/typeface.*` | Text drawing at Android em sizes: the face loaded into gpu2d's text, labels drawn a glyph at a time where `TextGrid` puts them, fitted with an ellipsis |
| `src/ui/text_grid.*` | Where a label's glyphs go: the whole-pixel line box, advances and baseline the stb_truetype atlas used, letter spacing, UTF-8 decoding |
| `src/ui/face_metrics.*` | The face's head, hhea, hmtx and cmap: line metrics and each character's advance |

## Maintainer tooling

| Path | Owns |
| --- | --- |
| `tools/vm/` | The headless Fedora 44 KDE guest for login-session tests (`docs/vm-harness.md`): `provision`/`kickstart`/`fetch`/`hostfiles`/`configure` (install and host-like setup), `machine` (qemu command line, PID lifecycle, overlays), `qmp` and `vnc` (screenshots, input), `guest` (SSH), `deploy` (build, install, ship), `control` (openSU's control channel in the guest), `cli` |
| `tools/nested_remap/` | Headless checks of the nested Gamescope in a private `kwin_wayland --virtual` (`host`: own runtime dir and bus, KWin scripts): `cli` runs openSU's nested command line under `WAYLAND_DEBUG` (`run`) with a window scenario (`child`: cycle, rapid, steam), optionally `kwin/focus_flap.qml` or the host stopping mid-run, and judges exit and protocol error (`verdict`); `restart` runs the real `CompositorSession` with `tests/session/nested_session_driver` as the child and SIGABRTs Gamescope until the restart limit |
| `pyproject.toml`, `uv.lock` | The one locked Python environment of the tools (`uv run --frozen`) |
| `tools/check/` | Building and checking openSU from a clean checkout, locally and in CI: `pins` (Lucent and gpu2d commits), `deps` (checkout by commit), `cmake` (configure with Clang and refuse any other compiler, build, ctest, install a component), `lint` (clang-format, clang-tidy over `compile_commands.json`, ruff), `container` (`all` inside the systemd-booted `packaging/ci` image as the calling user, for CI), `cli` (`deps`, `configure`, `build`, `lint`, `test`, `all`, `ci`) |
| `tools/appimage/` | The Linux AppImage: `config` (pins, names, layout, the signing key from the environment), `container` (the Ubuntu 24.04 build image and running in it), `inner` (in the container: build, install the default component; tests run in `tools.check ci`), `bundle` (the host-provided library list, copying the rest into `usr/lib`, runpaths, the glibc floor check), `appdir` (AppRun, desktop entry, icon), `pack` (appimagetool with the pinned static runtime, zsync update information, gpg signature), `smoke` (the AppImage on clean Fedora 44 and Ubuntu 24.04 containers with only the documented requirements) |
| `tools/screenshots/` | The README screenshots and the look-parity references (`docs/design/sdl3-gpu-shell.md` §5): a demo library of freely licensed games (`demo`, art pinned by sha256), openSU run hidden with no network (`offline`, `shell`), a fixed-volume `wpctl` stand-in for parity runs (`mixer`), the screens driven over the control channel, each reached on a `/state` field and taken once `artworkPending` is 0 (`scenes`, `readme` marks the README set), WebP output (`encode`), `--parity DIR` raw PNGs of every scene, and `compare` (per-scene mean, max and share of differing pixels, difference images, non-zero exit over thresholds); credits in `docs/screenshots/CREDITS.md` |
| `tools/download.py` | The one checksum-verified download the tools share |
| `tools/podman.py` | Container images the tools build, tagged by the hash of their recipe, and the worktree git dir a container must also mount |
| `packaging/ci/Containerfile` | The CI image: Fedora 44 (the fork's podman build release) with openSU's and the fork's build dependencies and systemd as PID 1, so tests get a user manager |
| `packaging/appimage/Containerfile` | The AppImage build image: Ubuntu 24.04 (the glibc 2.39 floor), openSU's and the fork's build dependencies, wayland, wayland-protocols, libxkbcommon, pixman and libdrm headers newer than 24.04's from pinned tarballs, SDL3, SDL3_ttf and SDL3_image built from pinned release tarballs, uv |
| `packaging/appimage/signing-key.asc` | The public half of the key release AppImages are signed with (private half: the `OPENSU_APPIMAGE_SIGNING_KEY` repository secret) |
| `packaging/opensu.desktop.in`, `packaging/opensu.svg` | The application menu entry and openSU's icon, installed to `share/applications` and `share/icons/hicolor/scalable/apps`; the AppImage carries both at its top |
| `.github/workflows/ci.yml` | CI on every push and pull request: `tools.check ci` (`all` in the `packaging/ci` container, fork built natively), and `tools.appimage all` (build and smoke) on Ubuntu 24.04 |
| `.github/workflows/release.yml` | A `v*` tag: the signed AppImage built and smoke-tested, then a GitHub Release with the AppImage, its `.sha256`, `.zsync` and the sign-in `.xpi` |

## Tests and docs

- `tests/<area>/`: one ctest suite per owner, through the shipping code.
- `tests/support/`: shared fakes: `fake_steam.hpp` (a DevTools Steam), `release_server.hpp` (a release API and asset host), `buildbot_server.hpp` (a libretro buildbot nightly folder), `zip_fixture.hpp` (zips built in memory), `loopback_server.hpp`, `fake_runner.hpp` (`opensu-retro`'s control channel); `scratch_dir.hpp`, a test's own empty directory under the test scratch.
- `tests/plugin/`: the plugin host through the shipping runtime (`plugin_rig.hpp`), the shipped Eden plugin against fixture files.
- `docs/design/`: designs (`lua-plugins.md`: the Lua plugin host, phase 1 built, later phases proposed).
- `docs/re-frontier.md`: what is recovered from iiSU. `docs/reference/` (private `opensu-re`
  checkout): the evidence and the spec built on it.
