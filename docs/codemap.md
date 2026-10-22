# Code map

Where each concept lives. Read this before placing code; update it in the change that adds or
moves an owner. The UI replicates iiSU, so UI owners cite the iiSU function they reproduce and the
evidence is in `reference/iisu/` (the private `opensu-re` repository, checked out at the
gitignored `docs/reference/`).

An OS-specific concept has one owner whose backend is one source file per OS, picked in that
owner's `CMakeLists.txt` (`CMAKE_SYSTEM_NAME`), so callers never test the OS: process launching,
pads, the shell over a game, Steam's modes, the user directories, the buildbot folder, run-time
library loading and the battery. X11, Gamescope and `src/session` are Linux's alone.

## Entry and composition

| Path | Owns |
| --- | --- |
| `src/config/arguments.*` | The command line as typed values (`--render`, `--keyboard`, `--session`: openSU is the login session, `--hidden`: the shell with an unmapped window, no Gamescope session, no pads, free control port, no Steam client unless `--steam`: `startsSteam()`) |
| `src/main.cpp` | Starts the nested session or the shell; `--render FILE` renders one frame headless |
| `src/app/shell_app.*` | Composition: catalog, controller reader, Steam client, the drawn shell, frame loop; builds every integration from `IntegrationsInForce` and applies a change through `IntegrationSwitch`; plays the UI sounds at the input events that iiSU plays them at; hands the left stick's directions to an open daisy wheel (`wheelTakingStick`); streams artwork every loop pass, drawn frame or not |
| `src/app/game_launch.*` | One launch at a time on its thread, which it owns: the game launched, whether it runs, cancel and close through `Handoff`; destroyed, it closes the game and joins the thread |
| `src/app/integrations_in_force.*` | The `settings::Integrations` in force for this run, from the saved choices, `--session` and the Steam modes the build has (`steam::clientModes`); the modes the player can pick (`modesOffered`); saving a new choice; what the catalog lists, the Steam mode, whether the shell guards openSU's own Steam (Managed, Isolated) |
| `src/app/integration_switch.*` | Applying the integrations in force: at start and on each change, only what differs (catalog `replace`, plugins host, standalone emulators, pad and Big Picture guards, Steam's windows hidden while Managed or Isolated Steam is Ready (`service`, each frame), Steam stopped and started in its mode on a worker, Isolated's four kept virtual pads made before and let go after) |
| `src/app/integration_options.*` | The optional integrations' ids, labels and notes, and the switch each one is in `IntegrationChoices` |
| `src/app/first_run_chooser.*` | The first-run chooser's controller: opens when no choices are saved, A toggles, Done saves |
| `src/app/integration_settings.*` | Settings › Integrations: ROMs and libretro always on, a switch per integration, Steam mode (Normal, Managed, Isolated; no Normal in a session), and for Isolated what Steam sees of the controllers ("Steam sees") |
| `src/app/shell_window.*` | The shell's SDL window on gpu2d (`GpuWindow` and `Canvas`): resizable, translucent, 960x600 minimum, unmapped without vsync for `--hidden` and `--render`; logs the device, driver and translucency; the frame's keys, typed text, pointer and resize from SDL events; a frame presented at most 60 a second, or drawn offscreen and read back; the monitor's size and refresh and filling it for the Gamescope overlay |
| `src/app/sdl_keys.*` | SDL scancodes onto the shell's key codes (`input::keys`) |
| `src/app/control_bridge.*` | The hand-over between the control channel's threads and the loop: queued input, reload and close requests, the published `ShellSnapshot`, the frame handshake |
| `src/app/game_screen.*` | The window's relation to a running game: the platform's overlay (`GameOverlay`), or the hidden window flag; puts the shell back in front (`takeFocus`); interactive while a menu or page is up (keyboard taken, the game's pads rested), passive while the chord list is up (shown, the game keeps the keyboard and the pads), hidden otherwise |
| `src/app/top_bar_clock.*` | The top bar's clock set again on each minute boundary, and the battery read on the same tick |
| `src/app/guide_menu_controller.*` | The Guide menu's buttons and what each entry does (sections, Devices, Downloads, Settings, close game, quit, the power button and list: `host::Power` with a second press for restart and shut down, `host::SessionMode` for Switch to session mode (second press), Install session mode (opens the dialog) and, in a login session, Switch to desktop) |
| `src/app/session_install_controller.*` | The Install session mode flow: the dialog, then the password on the on-screen or physical keyboard, the root step on a worker, a wrong password asking again, the password wiped once sudo has had it |
| `src/app/keyboard_pad.*` | The pad buttons on the on-screen keyboard, the one implementation search, `path_editor` and the session install share: the regular grid's D-pad walk and the daisy wheel's face buttons, LB/RB delete and space, LT/RT sets, d-pad cursor (`KeyboardOutcome`); `directionOf` |
| `src/app/quick_menu_controller.*` | The quick menu's rows and buttons: volume, mute, output, brightness, Bluetooth, controller batteries, Devices, Game settings (opens the settings tabs) for a game on `opensu-retro` or wherever frame generation can be set, Save state, Load state and Reset game for a game on `opensu-retro` (Load and Reset on a second press), Close game |
| `src/app/retro_session.*` | The shell's side of `opensu-retro`'s control channel: each launch's `OPENSU_RETRO_CONTROL` file under the runtime dir, the port read from it, `/status`, save, load and reset over `lucent::http` loopback with the toast each comes to (`perform`), live video settings (`setVideo`, `POST /video`), the load failure a runner leaves handed over and the file removed; the launch's `.settings.json` beside it |
| `src/app/retro_launch.*` | A launch on a built-in core: video settings resolved from the setting layers into the per-launch settings file and `--settings` added (`builtInOf`, `prepareRetroLaunch`); a running game's video settings resolved again (`retroVideoOf`) |
| `src/app/game_settings.*` | The shell's setting layers: `game-settings.json` loaded (emulator picks migrated first), saved with a toast on failure; `sourceText` ("From Super Nintendo", "Set for this game"), `layerName` ("Global", "Super Nintendo", "Snes9x (built-in)", "this game") |
| `src/app/game_settings_page.*` | The game settings tabs (Global, the game's console, Game) from the quick menu and the details page: the video rows on a built-in core, the Frame generation group (Mode, then the values the mode uses; only "Needs Lossless Scaling installed through Steam" without the DLL) where frame generation can be set; whether a game has anything there (`offersGameSettings`); Override checkboxes (tick copies the inherited value into the layer, untick deletes the key), inherited value and source per row, the value lists and the shader preset picker, each change saved and applied live to the running game |
| `src/app/frame_generation.hpp`, `.cpp`, `frame_generation_gamescope.cpp` (Linux), `frame_generation_none.cpp` (elsewhere) | Frame generation for the game in front: resolves its `framegen.*` keys and hands them, with the player's `Lossless.dll`, to the platform's output whenever the game in front or a value changes; off with no game in front or no DLL; the output inside Gamescope is the fork's `GAMESCOPE_FRAMEGEN_*` root atoms, none elsewhere or outside Gamescope (`openFrameGenOutput` returns null and the settings are hidden) |
| `src/app/layer_settings.*` | Settings › Emulators' layer pages: Global, each console (emulator pick, video) and each built-in core (video, the only place the core layer is set); "not set" steps clear a key; the Shader preset row opening the picker; Clear per layer; reset of every game's own emulator pick |
| `src/app/shader_preset_picker.*` | The shader preset picker: Not set, None, then the downloaded slang pack's folder tree (subfolders with preset counts, then presets); `presetLabel` ("crt › crt-royale") |
| `src/app/game_end.*` | The toast for a launch that is over: a runner's load failure, `<title> crashed (SIGSEGV)` for a death by a signal the player did not send, `<title> launch cancelled` for a launch cancelled from its panel, the launch's failure, or `<title> closed` |
| `src/app/devices_controller.*` | The Devices page: Bluetooth, Controllers, Audio output and Display tabs and what their rows do |
| `src/app/downloads_board.*` | The Downloads page's model: Steam's queue, the store install, update or uninstall, the emulator, core or shader pack download (`DownloadKind`) and the arcade romset list fetches (`romsets:<core id>`, in fetch order, no buttons) merged each frame into rows with state, order and the actions each owner allows; ended rows kept until quit. Pure |
| `src/app/downloads_controller.*` | The Downloads page's controller: feeds the board (the romset lists from `RomsetLists`, a failure's reason and retry time; each list's end toasted and the catalog read again), fills the page, sends a pressed button to the row's owner (`Installs`, `EmulatorDownloads`, `steam::DownloadControl` on a worker) |
| `src/app/host_services.*` | Power, session mode, Bluetooth and backlight backends for this machine; the one place a hidden run is made read-only |
| `src/app/bluetooth_session.*` | BlueZ reads off the loop and one change at a time, with notices; pairing is pair, trust, connect |
| `src/app/controller_roster.*` | The pads connected now in player order, with battery and held buttons (the live button test) |
| `src/app/audio_outputs.*` | The sound outputs and the default one, through `SystemVolume` |
| `src/app/brightness_control.*` | Display brightness through the backlight, absent without one |
| `src/host/` | System services over `busctl`, `systemctl` and `bluetoothctl` behind fakeable seams: `runner` (run or hold a program), `bluetooth` (BlueZ), `power` (logind: sleep, restart, shut down), `session_mode` (install session mode, switch to it and back through the selector, `sudo` and the logout call; see Session and processes), `backlight` (sysfs read, logind write), `secret.hpp` (wiping a password buffer) |
| `src/app/layout_picker.*` | START: the options panel's buttons (layout cards, icon size, pin, sort, source, installed, hidden, search row) and saving what is chosen |
| `src/app/preferences.*` | The loaded `settings::Settings` and saving them, with a toast when the file cannot be written |
| `src/app/search_controller.*` | The search panel's pad buttons (the results zone, then `keyboard_pad`) and keyboard text; the typed text is the view's search |
| `src/app/context_menu_controller.*` | Opening the focused tile's menu and what its entries do (launch, update, details, hide/unhide, open, refresh, sign in, uninstall on a second press) |
| `src/app/details_controller.*` | The details page's buttons: open for the focused game, Play or Install, Update, the emulator pick and where it comes from (Automatic (romset), From <console>, Set for this game), a built-in core game's video rows with the source of each kept value, the Game settings button for any game the tabs have something for (opens the settings tabs; reads "Inherited" or the override count, frame generation keys included), Clear game settings, Download <emulator> (start or cancel the emulator download), Wine prefix (a Windows install's own or the shared one, saved), Hide, Uninstall (two presses), Controls (the game's mapping), Back; keeps the page current as the library changes |
| `src/app/update_request.*` | Update on the details page and the tile menu: one press starts `Installs::update` and tells the player, or tells why not |
| `src/app/steam_update_job.*` | A Steam update asked for, as an `InstallJob`: starts the pending update as Steam's library Update button does (`DownloadControl::moveToTop`, `ResumeAppUpdate`) and ends; Steam's queue row carries it |
| `src/app/uninstall_prompt.*` | The two presses of Uninstall the details page and the tile menu share: the first asks `Installs` what goes (or says why not), the second starts it |
| `src/app/breadcrumb_trail.*` | The one computation of the breadcrumb trail from the shell's state (section, folder, search, filters, open game, Settings, Devices, Downloads) |
| `src/app/settings_controller.*` | The Settings screen's controller: rows per category, input routing, saving through `Preferences`, folder chooser and path entry; Install folders shows each store's resolved folder (override, default or the store's own); Steam libraries, Wine, Plugins and the Library page's Steam rows only with their integration on |
| `src/app/plugin_service.*` | Which plugins run (bundled unless turned off, user plugins only when turned on with their exact permissions granted), refresh requests, play-time reports applied to the library, the stop toast; no plugins host at all while Lua plugins are off |
| `src/app/plugin_settings.*` | Settings › Plugins: a toggle and an access row per plugin, the consent dialog before a user plugin runs |
| `src/app/emulator_downloads.*` | The emulator download job for official AppImages, built-in emulators' cores and the slang shader pack: one download at a time on a thread with progress and cancel, the update check (shows, never installs; the buildbot listing read once for every core), removal of what openSU downloaded; news handed to the loop in `service`; with standalone emulators off, only built-in cores |
| `src/app/emulator_settings.*` | Settings › Emulators: each emulator's install state, source and version (a core's: libretro buildbot and its build date, or the folder it was found in) with Download / Update / Remove (two presses), Check for updates, the rows opening the layer pages (`layer_settings`), the Shader presets row (download, progress, update, two-press Remove); each chosen built-in core's BIOS (mode: HLE built in, open BIOS, not needed, missing or your file; each core-info file present or missing in the runner's system folder) |
| `src/app/wine_settings.*` | Settings › Wine: the Wine runner (Automatic or one found), the shared prefix, how many games have their own, Steam left to Steam; `prepare` gives a launch the picked runner and prefix through `library::wine` |
| `src/app/steam_library_settings.*` | Settings › Steam libraries: the libraries with path, free space, games and size; add, make default, remove an empty one, on a worker thread; read-only from `libraryfolders.vdf` while Steam is not running |
| `src/app/path_editor.*` | Choosing a folder on the chooser or typing a path; one request at a time, refusals shown with their reason |
| `src/app/artwork_delivery.*` | Hands saved artwork, store contents and UI sounds to the shell and sound player |
| `src/app/volume_control.*` | The volume actions onto `SystemVolume` and its backend; polling; the OSD listener |
| `src/app/shortcut_editor.*` | The remap capture: pick an action, press the new combo or pad chord (heard by `ChordCapture`), refusals; restores every shortcut or only the chords |
| `src/app/chord_capture.*` | Hears one pad chord from raw pad events: the first button held, then another; B alone cancels on release (the shortcut and mapping editors) |
| `src/app/mapping_editor.*` | The button mappings (`docs/issues/per-game-button-mappings.md`): loads and saves them through `MappingStore`, resolves a game's at launch (`forGame`, `mappingKeys`), and drives the mapping page for a game's, an emulator's or the global map (pick lists, chord capture, refusals, Steam Input); Settings › Controls' mapping rows |
| `src/app/game_overlay.hpp`, `game_overlay_gamescope.cpp` (Linux), `game_overlay_none.cpp` (elsewhere) | The shell over a game, per OS, picked in `src/app/CMakeLists.txt`: Gamescope's overlay (`session::GamescopeOverlay`) and the game-time keys (`session::GameKeys`) on Linux; none elsewhere, where the window is lowered and raised (`openGameOverlay` returns null) |
| `src/app/pad_owner.*` | Who has the pads (`gamepad::Claim`) from the game state and Gamescope's focus: the shell in front holds them inside Gamescope, another window gets them and the shell hears only Guide (`admit`); a game's mapping as the pads' `ForwardMap` and its chords in the `ChordSet`, released together; which pads a run reads (`padsFor`: the system's, or a hidden run's `<data>/hidden-pads`); the shell holds them only against openSU's own (Managed, Isolated) Steam; a `PadSource` that cannot grab collapses the claims to Free and Away |
| `src/app/focus_follow.*` | Follows Gamescope's focused window: closes Big Picture whenever it takes the focus and puts the shell back; Guide behind another window brings the shell back (`guideReturns`); leaves Big Picture alone unless Steam is openSU's own (Managed, Isolated) |
| `src/app/chord_settings.*` | Settings › Guide chords: one row per action a chord can do, its chord, the capture through `ShortcutEditor`, restore the shipped chords |
| `src/app/shortcut_router.*` | Keys, pad events and game keys onto shell events and actions through `Shortcuts`; every action that is not a button is done here (quick menu, volume, quit, and the game actions, with a toast when no game runs) |
| `src/app/frame_png.*` | The drawn shell as PNG bytes, for the control channel's frame and `--render` |
| `src/app/panel_flow.*` | The launch/install panel flow taken out of `shell_app.cpp` |
| `src/app/launch_mailbox.*` | What the launch and sign-in threads hand the loop: the game-running flag, the latest progress line, the next toast |
| `src/app/pointer_router.*` | The mouse onto the shell's actions: hover focus on a moved pointer, left click = focus + A (or the dock, page, breadcrumb and panel-button action), right click = the target's context menu (never B), wheel = a pad step; asks `PointerHost` (`ShellApp`) what is under the pointer |
| `src/app/control_channel.*` | Loopback HTTP control channel (`lucent::http::Server`): shell state, input (`guide+a` holds buttons together, `guide 1500ms` holds them that long, `up+right stick` makes the directions the left stick's), frames, `/signin/<store>[/start]`; `/state` includes `armed` (a two-press button waiting), `mapping` (the mapping page: `board`, `picking`, `listening` or empty), `mappingFocus` (its focused item id, or the focused tile's label while picking) and the Downloads page's rows; `firstRunOpen` while the first-run chooser is up |
| `src/app/sign_in.*` | Store sign-in steps: open the sign-in page in the default browser, finish with the code (GOG token, Epic via `legendary auth --code`) |
| `src/app/launcher_status.*` | The launcher badges' states from the Steam client, its downloads and the catalog's store statuses |
| `src/app/install_job.*` | The store-neutral job on a game (each install, update and uninstall): its thread, the latest report the loop takes, its state for any reader (`status`), the licence answer, the player's cancel |
| `src/app/cli_install_job.*` | An install by a downloader program: run it, turn its logged progress into reports, fail with its reason; the base of the Epic and GOG jobs |
| `src/app/steam_install_job.*`, `epic_install_job.*`, `gog_install_job.*` | One Steam install (walks Steam's installer, follows its queue); one Epic install or update (`legendary install`, `legendary update`); one GOG install or update (`gogdl download`, `gogdl update` over the recorded folder: token handed over and taken back, Linux or Windows build, the install and its build recorded) |
| `src/app/installs.*` | The installers and updaters by store and the uninstaller, one install, update or uninstall at a time, the last one's state (`Work`) and its cancel; which stores install; the refusals updates and uninstalls share (ROM, not installed, running, busy), the update's own (none known, Steam not running, no GOG record) and what an uninstall removes (`removal`); each store's own install folder (`ownFolders`: GOG under the data dir, legendary's, Steam's default library) |
| `src/app/uninstall_job.*` | One store game uninstalled as an `InstallJob`: Steam through `steam::Uninstaller` until its manifest goes, Epic with `legendary uninstall <app> -y`, GOG by deleting the recorded folder only inside its gogdl root, then forgetting the record; never a Wine prefix or a ROM |
| `src/config/config.*` | The one reader of the environment, into typed immutable config (cache, data, config and runtime dirs included, from `Locations`, the XDG bases plugin paths expand from, legendary's config dir, the logind session id); where the Gamescope fork binary is (`gamescopeBeside`, relative to `/proc/self/exe`) and the libretro runner beside the executable (`retroRunnerBeside`); librashader (`librashaderBeside`, `<libdir>/opensu/librashader.so` beside the runner); the runner's saves and system folders (`savesDir`, `retroSystemDir`); the runtime dir (`runtimeDir`) and the runner's control file (`retroControl`, from `OPENSU_RETRO_CONTROL`); the X display (`x11Display`), Gamescope's Wayland socket (`gamescopeWaylandDisplay`) and the desktop's session bus (`sessionBus`) Isolated Steam's sandbox is built from |
| `src/settings/settings.*` | The player's saved preferences (Library layout, dock pin, icon size, sort and filters, hidden games, last-played times, Settings-screen choices, interface scale, the on-screen keyboard style, install folders, the Wine runner and the games with their own Wine prefix, shortcut remaps, plugins turned on or off with the permissions granted) as JSON under the config dir; defaults on a missing or corrupt file; the integration choices (`integrations`), absent until chosen |
| `src/config/locations.*` | Where openSU's config, data, cache and runtime directories are on this OS, through Lucent's `platform` resolvers (`$XDG_*/opensu` on Linux; `openSU` under `%APPDATA%`, `%LOCALAPPDATA%`, `~/Library` or `TMPDIR` elsewhere); the folder name per OS (`OPENSU_LOCATION_NAME`, from CMake) |
| `src/settings/integrations.*` | The integration choices and the one rule for what is in force from them (`Integrations::inForce`): ROMs and libretro always, the rest only when chosen; Steam Off, Normal, Managed or Isolated among the modes the build has (`modesOffered`), no Normal in a login session; Isolated's controller choice (`SteamPads`: openSU's virtual pads, the default, or every controller) with its label and note |
| `src/settings/setting_layers.*` | Setting layers (Global, System, Emulator = the core layer, Game), `LayerKeys`, `layersOf`, one resolve rule with the source layer (`Resolved::from`); missing key inherits, clear deletes |
| `src/settings/setting_choices.*` | A layered setting kept as one of a fixed list (`ChoiceKey`, `ChoiceText`): its choices, labels and default place; `choiceIndex`, `choiceLabel`, `choiceIndexOrDefault` |
| `src/settings/framegen_settings.*` | Frame generation settings `framegen.mode` (off, fixed, adaptive) / `.ratio` / `.target` (`display` or fps) / `.max` / `.tolerance` / `.performance` / `.flow-scale`, every default off; which keys a mode uses (`frameGenKeyUsedIn`); `resolveFrameGen` to `FrameGenSettings` in the fork's atom units (thousandths, percent) |
| `src/settings/video_settings.*` | Video settings `video.aspect` / `video.integer` / `video.sampling` / `video.filter` / `video.shader` (slang preset path in the pack, `""` None; `replacedByShader`, `resolveShader`): choices and labels (`videoKeys`), `resolveVideo` to `VideoSettings`, the runner's per-launch file and `POST /video` body (`videoJson`, `parseVideoJson`) |
| `src/settings/game_settings_store.*` | `game-settings.json` under the config dir: every layer's keys (`global`, `systems`, `emulators`, `games`), written atomically; damaged entries left out |
| `src/settings/emulator_pick_migration.*` | One-time move of `settings.json`'s `emulators` / `systemEmulators` picks into the game settings, the old keys then dropped |
| `src/settings/mapping_store.*` | `mappings.json` under the config dir: every scope's `ButtonMap` and the Steam Input switch, written whole and atomically; a damaged file or entry warned and left out |
| `src/settings/install_folders.*` | The one resolver of where a store installs: a store's override, else the default, else none (the store's own choice); `resolve` adds the store's own folder and says which of the three applies; validation (exists, writable) |
| `src/fileio/atomic_write.*` | Whole-file writes through a `.part` file and a rename |
| `src/zip/zip_archive.*` | The one zip reader: end record, central directory, local header offset, checked extraction (256 MB unless the caller allows more), an in-memory archive, and the CRC-32 (`crc32Of`) zips and libretro listings are checked with; used by artwork, core downloads and arcade romsets |
| `extension/opensu-signin/`, `extension/CMakeLists.txt` | Firefox/Zen WebExtension that hands a GOG or Epic sign-in code to the control channel; packed into `opensu-signin.xpi` and installed beside the assets |

## Session and processes (G003, G004)

| Path | Owns |
| --- | --- |
| `src/session/compositor_session.*` | Re-running openSU inside its own Gamescope when not already in one, without the Gamescope WSI layer so its window reaches Gamescope through Xwayland with alpha: nested at the monitor's size on a desktop, top level on DRM at the native mode for `--session`; starting a fresh Gamescope after an abnormal end nobody asked for |
| `src/session/restart_budget.*` | How often a crashed compositor is started again (3 restarts in any 2 minutes) |
| `src/host/session_mode.*` | Session mode, the one owner of switching sessions: `installed()` (an entry for this openSU in a directory SDDM reads, and the selector), the root step (`sudo -S -k` with the password on stdin, one `install-session.sh`), Switch to session mode (selector `opensu`, then KDE's `org.kde.Shutdown.logout`) and Switch to desktop (selector `restore`, then stopping the Gamescope scope); read-only wrapper for hidden runs |
| `src/host/login_session_end.*`, `src/session/login_session.*` | What the next login is when the login session ends: an abnormal end (Gamescope non-zero, by a signal, or within 30 s) runs `sudo -n <selector> restore` so SDDM autologin cannot relogin in a loop; a logout or shutdown signal and a deliberate Switch to desktop (the note) leave it alone; `LoginSession` composes the top-level `CompositorSession` with the judgement and the desktop fallback |
| `src/host/desktop_request.*` | The note Switch to desktop leaves in the runtime dir (it marks the end deliberate), and, as the fallback when the selector is not installed, `startplasma-wayland` run in the login session's place once Gamescope has ended |
| `packaging/install-session.sh`, `packaging/opensu-session-select.sh` | The two root-run POSIX sh scripts, installed to `<prefix>/libexec/opensu/`: the one-sudo installer (session entry, `/usr/local/libexec/opensu-session-select`, `/etc/sudoers.d/zz-opensu-session-select`) and the selector (`opensu` or `restore`: SDDM drop-in `zz-opensu-session.conf` or the last session in `state.conf`) |
| `src/session/gamescope.*` | The Gamescope command line, nested (`--close-focused-window` included) or top level (`--backend drm`) from `Output::native()` |
| `packaging/opensu-session.desktop.in` | The Wayland session entry, installed to `<prefix>/share/wayland-sessions/opensu.desktop` (`install-session.sh` copies it where SDDM reads) |
| `cmake/Gamescope.cmake` | Building the pinned Gamescope fork in podman, or natively in an environment that already has its dependencies (`OPENSU_GAMESCOPE_BUILD_ENV`: CI's Fedora container, the AppImage container), with `OPENSU_GAMESCOPE_MESON_OPTIONS` for a distribution lacking some (commit, build steps, staging and install path); `OPENSU_BUILD_GAMESCOPE`. `cmake/GitFetchPin.cmake` fetches a new pin into an existing clone (Gamescope's and librashader's), `cmake/GamescopeImage.cmake` builds the image if absent, `cmake/GamescopeLddCheck.cmake` checks the staged binary's libraries. The fork's own features: `--close-focused-window`, root `GAMESCOPE_HIDE_STEAM_CLIENT`, and frame generation for the focused window (`src/FrameGen/`: `FramePacer` count, timestamps, adaptive target, maximum, tolerance hysteresis, low-fps floor and per-vblank pick; `IFrameSynthesizer`; `LosslessSynthesizer` the MIT generator running the LSFG 3 shaders, `LosslessShaders` the resource ids and interface check of the DLL's SPIR-V, `PeResources` the DLL's PE resources and FileVersion, `SpirvInterface` SPIR-V reflection, `LosslessPipelines` pipelines, set layout and samplers, `LosslessImages` level sizes, flow-scale clamp and intermediate images, `ComputeImage` any-format compute images; `LinearBlendSynthesizer` the cross-fade test choice; root `GAMESCOPE_FRAMEGEN` and `GAMESCOPE_FRAMEGEN_MODE`, `_RATIO`, `_TARGET`, `_MAX`, `_TOLERANCE`, `_DLL`, `_GENERATOR`, `_PERFORMANCE`, `_FLOW_SCALE`; `src/GpuTimestamps` GPU timestamp averages for the composite and the generator) |
| `packaging/gamescope-build/Containerfile` | The Gamescope build image: host's Fedora release plus Gamescope's build dependencies |
| `src/session/monitor.*` | The output's size and refresh, from SDL's display modes (X11 video): the primary monitor before the window opens, the window's display after |
| `src/session/gamescope_overlay.*`, `overlay_mode.hpp` | openSU's window as Gamescope's overlay over a running game: hidden, passive (drawn, input left to the game) or interactive (`STEAM_INPUT_FOCUS`); `OverlayMode` itself is window-system neutral |
| `src/session/game_keys.*` | The shortcuts that work while a game has the keyboard (Guide, volume), matched against the `Shortcuts` table from XInput2 raw keys |
| `src/session/gamescope_windows.*` | Which processes own a window Gamescope would show (tells the handoff when a game is on screen), and the focused window's changes from the root's properties |
| `src/session/root_properties.*` | Writing CARDINAL and UTF8_STRING properties on the X root, how openSU asks its Gamescope fork for things; one connection per owner |
| `src/session/steam_window_hiding.*` | Hiding Steam's own client windows (Big Picture, steamwebhelper) from Gamescope's focus and composite: the fork's root `GAMESCOPE_HIDE_STEAM_CLIENT`, set by `IntegrationSwitch` while Managed or Isolated Steam is logged on |
| `src/session/window_focus.*` | Whose window Gamescope has focused (openSU, the launched game, Big Picture, another window), as pure rules over `GAMESCOPE_FOCUSED_WINDOW` and `GAMESCOPE_FOCUSABLE_WINDOWS` |
| `src/launch/process_launcher.hpp`, `process_launcher_systemd_scope.cpp` (Linux), `process_launcher_unavailable.cpp` (elsewhere) | Process launching per OS, picked in `src/launch/CMakeLists.txt`: one transient systemd user scope per launch, its program started with no signal blocked; stopping it ends the whole tree; how the started process ended (`exitStatus`); elsewhere a launch refuses with "not available on <OS>" until that OS's backend lands |
| `src/launch/handoff.*` | Starting a game, reporting its progress until it shows a window, hiding the shell until it ends; how a game in a `ProcessLauncher` ended (`Hooks::ended`), unless force-closed; a cancelled or closed Steam launch followed until Steam lets go (`endSteamLaunch`) |
| `src/launch/process_tree.*` | Finding processes by command line, ending trees |
| `src/launch/command.*`, `argv.hpp`, `child_environment.*` | The environment every started program gets (openSU's own less `DISABLE_GAMESCOPE_WSI`, which is the shell's alone), short-lived children, a child streamed line by line, its stdout captured or fed one line on stdin (`runCapturedWithLine`, the sudo password) (own `opensu_command` target, so `library` can run `legendary`), executable lookup, exec argv |
| `src/launch/steam_gate.hpp` | What a Steam launch waits for from the owned Steam client; whether a Steam game shows in openSU's Gamescope (Managed, Isolated) or on the desktop |
| `src/launch/launch_progress.*` | A launch's stage before its window (waiting for Steam, updating, starting, loading, cancelling) and its line of text |
| `src/launch/game_windows.hpp` | What the handoff asks the display: does the game show a window yet |
| `src/steam/client_modes.hpp`, `client_modes_scoped.cpp` (Linux), `client_modes_normal.cpp` (elsewhere) | Which Steam modes the build's client offers (`clientModes`): Normal, Managed and Isolated on Linux; Normal alone elsewhere |
| `src/steam/client.*` | The Steam client openSU uses, in its mode: Managed starts its own in the background in openSU's scope and ends it; Isolated the same inside the bwrap sandbox with its Secret Service relay; Normal attaches to the desktop Steam or starts one on the desktop outside openSU's scope; readiness, state |
| `src/steam/sandbox.*` | Isolated Steam's bwrap command line (`sandboxArguments`): the player's files and login, openSU's Gamescope X socket as the only display, an empty runtime dir with only Gamescope's and the audio sockets, private /tmp and /dev/shm, the GPU nodes (`gpuNodes`), the input nodes chosen (`SandboxDevices`: openSU's four kept virtual pads, `isolatedPadSlots`, or the host's /dev/input) and the X display number (`x11DisplayNumber`) |
| `src/steam/secret_bus.*` | Isolated Steam's only session bus: `xdg-dbus-proxy` in `<session>-steam-bus.scope`, the desktop bus filtered to org.freedesktop.secrets, where Steam keeps its login |
| `src/steam/desktop_steam.*` | Detecting a Steam client running outside openSU, and the desktop Steam's pid (`~/.steam/steam.pid`, a live process named `steam`) |
| `src/steam/orphaned_steam.*` | Finding and ending a Steam scope (and Isolated's `-steam-bus` relay scope) left by an openSU that died |
| `src/steam/devtools.*` | Running JavaScript in Steam's SharedJSContext over Chrome DevTools |
| `src/steam/libraries.*` | Steam's library folders through `SteamClient.InstallFolder`: read, add, set default, remove (refused for Steam's own library or one that holds games) |
| `src/steam/downloads.*` | Steam's live download queue: parsing and reading it (place, bytes, rate, stage) |
| `src/steam/download_control.*` | Changes to Steam's queue through `SteamClient.Downloads`, the calls Steam's downloads page makes: pause and resume all, start now, move, remove; queueing an app's pending update |
| `src/steam/steam_json.*` | Reading values the SharedJSContext returns (sizes as numbers or strings); the one app id check (`isAppId`) before an id goes into a script |
| `src/steam/launch_activity.*` | Steam's game actions and running state per app, recorded in its SharedJSContext; cancelling an app's launch (`SteamClient.Apps.CancelLaunch`, the library's Cancel) |
| `src/steam/install_wizard.*` | Installing a Steam app through Steam's own installer, licence steps to the player |
| `src/steam/uninstaller.*` | Uninstalling a Steam app through `SteamClient.Installs.OpenUninstallWizard`, as Steam's library does |
| `src/steam/big_picture.*` | Closing Big Picture through `SteamClient.UI.ExitBigPictureMode`, as its own window does |

## Library

| Path | Owns |
| --- | --- |
| `src/library/game.*` | The launchable `Game` record |
| `src/library/catalog.*` | Building the catalog's providers from all sources (`game.*` holds `readProvider` and `assemble`, the merge); only the stores in force (`CatalogStores`), ROMs always |
| `src/library/catalog_loader.*` | Listing every store off the frame thread: one thread per provider, listings handed over as they arrive, `Loading` until a store's first one; `replace` swaps the providers when the integrations change, dropping a retired store's listings |
| `src/library/lossless_scaling.*` | The player's `Lossless.dll` from Lossless Scaling on Steam, found read-only in any of their Steam library folders (`findLosslessDll`, through `steam::Library::libraryFolders`), never copied |
| `src/library/steam.*`, `epic.*`, `gog.*`, `roms.*` | One source each; the stores' sources also say which installed games have an update (`Game::updateAvailable`); an arcade zip's emulator is picked by its romset (`romset_match`) |
| `src/library/gog_auth.*`, `gog_token.*` | GOG's OAuth sign-in and the saved token (`<data dir>/gog-token.json`); `writeOwnerOnly`, the one owner-only atomic write |
| `src/library/gogdl_auth.*` | The token as gogdl's `--auth-config-path` file, written for an install and read back |
| `src/library/wine.*` | Windows GOG and Epic installs under Wine: the runners found (system `wine`, Proton builds in Steam's `compatibilitytools.d` and `steamapps/common`, read only), the shared prefix `<data dir>/wine-prefix` and a game's own `<data dir>/wine-prefixes/<store>-<id>` (`OwnPrefixes`), and `prepare`, which adds `--wine`/`--wine-prefix` to the launch |
| `src/library/gog_installs.*` | Where GOG files live under the data dir (`Paths`, a game's gogdl root `gameRoot`), the record of finished GOG installs with their build (add, find, remove), the build gogdl left on disk (`installedBuild`) and the folder an uninstall may delete (`ownedFolder`) |
| `src/library/gog_builds.*` | The build GOG offers now for an installed game, where gogdl reads it: the content system's builds for Windows, the product's Linux installer version for Linux; whether an install is behind |
| `src/library/install_log.*` | What legendary and gogdl log while installing: progress fraction, ERROR/CRITICAL lines |
| `src/library/sections.*` | The dock's sections (Home, Library), the active one and L1/R1 cycling with wrap; Library's layout modes and their keys |
| `src/library/shelf.*` | What the grid holds: Home's installed store games (`homeShelf`); Library's launchers, All games and consoles (`libraryShelf`); a console's ROMs, a launcher's library, the combined library; moving between them and between sections |
| `src/library/library_query.*` | The one owner of what the library shows: the search ranking, the filters (installed, source, hidden), the sort, `HiddenGames`; Home, Library, folders and All games take their games from `visibleShelf` |
| `src/library/text_fold.*` | Case, accent and white-space folding titles are compared in |
| `src/library/emulator_choice.*` | The `emulator` setting at the Game and System layers (`pickFor`, `EmulatorSource`: game, romset, system, search): `applyEmulatorPicks` points the game's `launch` and `emulator` at the game's pick, else the system's unless the game's arcade romset does not match it; `unavailable` when it is not installed or cannot start; `layerKeysOf` (a game's layer keys) |
| `src/library/play_history.*` | When openSU last launched each game, for the recent sort of stores that do not say |
| `src/library/play_time.*` | `PlayTimeReports`: each play-time source's latest report, summed into ROM games' play time |
| `src/library/emulator_game_list.*` | An emulator's own game list (its id, title, path, metadata per game) matched to openSU's games by path, then by unique title key |
| `src/library/titles.*` | The same title across stores: the comparison key, merged copies, preference order |
| `src/library/rom_titles.*` | A ROM's display title from its name: tags, release numbers and the ", The" order (`cleanTitle`); the one title cleanup, also the release-number rule the libretro matcher uses; trademark signs off a game's own title (`withoutTrademarks`) |
| `src/library/param_sfo.*` | The PlayStation PARAM.SFO reader (`sfoStrings`): its UTF-8 values by key |
| `src/library/dump_info.*` | What a game folder says about itself (`readDumpInfo`): a PS4 dump's `sce_sys/param.sfo` title and `icon0.png` |
| `src/library/switch_info.*` | What a Switch game file says about itself (`SwitchInfo`): Eden's key folders, the Control NCA (the base title's first), `control.nacp`'s title and the icon, kept under `<cache>/artwork/switch/` by file size and mtime; a listing's count of games titled and the failures with file and step |
| `src/library/switch_keys.*` | The player's Switch keys (`SwitchKeys`): prod.keys and title.keys parsed, the header, key area and titlekek keys a file lacks derived from the master keys as hactool does; never logged or written, wiped when dropped |
| `src/library/switch_package.*` | The files in an NSP (PFS0) or an XCI's secure partition (HFS0, with or without the key area in front), positioned reads, a common ticket's title key |
| `src/library/switch_nca.*` | One NCA (`Nca`): its header decrypted (AES-XTS, Nintendo's tweak), content type, program id, key generation, rights id; its RomFS section's key (key area or title key) and files (IVFC data level, AES-CTR) |
| `src/library/switch_crypto.*` | AES-128 ECB, CTR and XTS over libcrypto, and `KeyBytes`, key material wiped on destruction |
| `src/library/arcade_names.*` | Arcade short name to description: the libretro-database `.dat` parser and `NameDb`, the parsed names kept under `<cache>/names/` |
| `src/library/romset_dat.*` | An arcade core's ROM-set listing (`RomsetDat`): each set's ROMs (name, size, CRC-32, BIOS choice), `romof`, default BIOS, devices and disks, read from a Logiqx datafile or a MAME `-listxml` and kept in a line format of its own |
| `src/library/romset_dats.*` | The arcade cores' listings under `<cache>/romsets/<id>.tsv`: fetched over HTTPS from the catalog's `"romsets"` (files on their own origin, or a release's `.xml` zip, SHA-256 checked) with the bytes of all its files reported, due again after `romsetDatLifetime`, the old one kept when a fetch fails |
| `src/library/romset_lists.*` | The one owner of fetching those listings: off the library scan, one thread, each due listing in turn (`want`), every fetch's state for the session (queued, downloading with bytes, done, failed with why and `romsetRetryAfter`), `ended` counting ends so the shell matches again |
| `src/library/romset_match.*` | Which arcade core a zip's romset is for: its central directory read (`readZipMembers`), matched against each listing with the parent, BIOS and device zips of its folder (`matchSet`), verdicts kept in `<cache>/romsets/matches.json` by file stamps (`RomsetMatcher`, which never waits on a listing: one `RomsetLists` is fetching is `Pending`), the game's emulator, "No arcade core matches this romset", "Checking romset…" (cannot start yet) and the nearest core's gap set from them (`applyRomsets`) |
| `src/library/rom_systems.*` | Known systems: folder names, game files, the file a game folder starts, a system's label (`systemLabel`); the tables are `constexpr` (`name_list.hpp`) |
| `src/library/emulators.*` | Which emulator runs each system here, and its command line, from the catalog: openSU's downloaded AppImage, else a package on `PATH`, an AppImage in the usual folders, a Flatpak; a built-in emulator when its core is found and `opensu-retro` is there (`opensu-retro --core <core> [--option <key>=<value>]... <rom>`); every emulator of a system, installed or not (`options`), which a ROM's details page lists; with standalone emulators off, only the built-in ones are found |
| `src/library/emulator_catalog.*`, `assets/emulators.json` | The emulator catalog: id, name, systems, settings folder, launchers (programs, AppImage name, Flatpak id, arguments) and, for emulators with an official AppImage, the release API, asset pattern and download host; a built-in emulator names its libretro core (`"core": "mgba_libretro"`) instead of launchers, an arcade core also its ROM-set listings (`"romsets"`), a GPU core the core options that pick its Vulkan renderer (`"coreOptions"`, `CoreOption`); the systems it serves (`servedSystems`) |
| `src/library/core_target.hpp` | The libretro buildbot nightly folder and core library suffix for the OS and CPU built for (`coreTarget`, from `src/library/CMakeLists.txt`): `linux/x86_64` `.so`, `windows/x86_64` `.dll`, `apple/osx/arm64` `.dylib`, `android/latest/<abi>` `.so` |
| `src/library/libretro_cores.*` | Built-in emulators' cores: the lookup (openSU's `<data>/opensu/cores/`, then RetroArch's and the system's core folders), the buildbot's `.index-extended` parsed, a core zip downloaded on the buildbot's origin only, its one library (`coreTarget.librarySuffix`) taken out, checked against the listed CRC-32 (the library's, not the zip's) and written whole, its `<core>.info` (from the buildbot's `info.zip`, else RetroArch's info folders) and `<core>.json` record (build date, CRC-32), removal |
| `src/library/shader_pack.*` | libretro's slang shader pack (`shaders_slang.zip` from the buildbot): the latest build by HEAD (`Last-Modified`, size; the buildbot publishes no checksum), download checked by length and each member's CRC-32, unpacked into `<data>/opensu/shaders/slang/` with its record `slang.json`, removal, `presets()` (every `.slangp` with its folder) |
| `src/library/core_bios.*` | A core's BIOS files: libretro core info `firmware*` entries parsed, a core's `.info` out of the buildbot's `info.zip`, what a core does without them (`WithoutBios`: needed, HLE, open replacement, skipped; the catalog's `"withoutBios"`), which are in the system folder, the files a folder-scanning core picks by its own rule (`BiosScan`, `pcsxRearmedBiosIn`: PCSX ReARMed's size, signature and region check) and the mode the player reads |
| `src/library/emulator_release.*` | An emulator's latest official AppImage from its GitHub or Forgejo release JSON, downloaded beside the one in use, checked against the published SHA-256 (or recorded as seen, HTTPS from the project's host only), made executable and switched to |
| `src/library/downloaded_emulators.*` | The AppImages openSU downloaded under `<data>/opensu/emulators/<id>/<asset id>/`: the `installed.json` record (the atomic switch), old versions cleared, removal |
| `src/library/appimage_runtime.*` | Whether an AppImage can run here: its runtime's libfuse2 need, `/dev/fuse`, `fusermount`; the refusal names the install command for the distribution |
| `src/artwork/libretro_index.*` | Matching a ROM's name to libretro-thumbnails' box art listing |
| `src/artwork/arcade_dat.*` | The pinned libretro-database arcade listings (FinalBurn Neo, MAME 2016): download against size and CRC-32, parse, merge; the fetcher keeps the result in `NameDb` |
| `src/artwork/artwork_store.*` | Downloaded artwork on disk under the cache dir: paths, misses, listings, frame glyphs, the starter pack file, and UI sounds (`sound/<file>`, WAV or OGG) |
| `src/artwork/artwork_fetcher.*` | The background downloads (queue stages per game, on-screen games first, a settled game reported as not saved): Steam's CDN for Steam, gamesdb (else the library tile) for GOG, the key image URL for Epic, libretro-thumbnails for ROMs, iiSU's starter pack for console cards, iiSU's border pack for frame glyphs, the APK's sounds (`ApkAsset`) |
| `src/artwork/apk_archive.*` | iiSU's release APK read by HTTP ranges: central directory once, any entry by range with its CRC checked; `fetch` is find and extract with a found/missing/failed result (glyphs, sounds and nav icons use it) |
| `src/artwork/starter_pack.*` | iiSU's starter pack: its entry taken out of the APK against a pin, and a system's card (WebP, decoded and re-encoded by `gpu2d::image`) as PNG |
| `src/artwork/console_glyphs.*` | iiSU's frame glyphs: `border_pack.json` read once, a system's `logo_*.png` out of the APK |
| `src/artwork/iisu_assets.*` | Which APK entry is each UI sound, and all of them (`allApkAssets`) |
| `src/audio/effect.*`, `debounce.*` | iiSU's UI sounds openSU plays (`yp8`): effect to APK file name, the Domino cues and `dominoFor(tileCount)` (`xp8.a`), the 91 ms repeat rule for Enter/ExitConsolesApps and the Domino cues; pure (`opensu_audio_model`) |
| `src/audio/volume_backend.*` | PC output volume through wpctl or pactl (injectable runner), a read-only shadow for hidden runs, the install advice when no backend exists |
| `src/audio/system_volume.*` | Volume and mute state, steps, one listener, async polling of changes made elsewhere |
| `src/audio/sound_clip.*` | A UI sound decoded to PCM without a device: WAV through `SDL_LoadWAV`, OGG through libvorbisfile |
| `src/audio/sdl_sound_player.*` | The shell's UI sounds on SDL audio (`opensu_audio`): clips decoded on `load` with or without a device, the default device opened once by `open` (never in hidden runs or tests), one bound stream per effect, `play` restarts the clip through the debounce |
| `src/net/web_client.*` | HTTPS GETs over libcurl, with headers, and streamed file downloads with progress and cancel; shared by artwork, the stores and emulator downloads; `request` with a body cap (and, with `Guard::progress`, progress, cancel and a stall limit instead of a time limit), and `request` and `download` with each redirect hop checked (`Refusal`); `head` and `BodyFacts` (Last-Modified and Content-Length of a response); a URL's origin (`originOf`) |
| `src/vdf/` | Valve KeyValues parser |
| `src/device/battery.*` | The system battery's level and charging state from `SDL_GetPowerInfo` (every OS); a wireless controller's battery from sysfs by its unique id |
| `src/gamepad/event.*` | The shell's controls (`Button`, `Event`; `stick` marks a left stick direction) (`opensu_pad`) |
| `src/gamepad/pad_source.hpp`, `pad_source_evdev.cpp` (Linux), `pad_source_sdl.cpp` (elsewhere) | The pads interface every reader implements (`PadSource`: events, connected pads, sticks, claims, chords, kept virtual pads, whether it grabs), `PadInfo`, `Claim`, `OwnVirtualPads`; `openPadSource` picks the OS's backend in `src/gamepad/CMakeLists.txt` |
| `src/gamepad/sdl_pads.*` | SDL's gamepads as a `PadSource` (no grab, no virtual pads), caught with an event watch; SDL's buttons and axes as controls (`SdlPadState`, the translator's thresholds); built on Linux too |
| `src/gamepad/pad_translator.*` | One physical pad's evdev events as the virtual pad's and as controls; analogue triggers as L2/R2 with hysteresis (`triggerPressAt`, `triggerReleaseAt`) and the stick's direction point (`stickDirectionAt`); where the two sticks stand (`Sticks`, `sticks()`); what reaches the virtual pad passes the pad's `ChordGate` and the game's `ForwardMap` (remapped buttons, a game chord's outputs held while it is); the shell's controls are never mapped |
| `src/gamepad/chord_gate.*` | The one chord rule (`Chord`, `ChordTriggers`, `ChordGate`): a bound modifier is held back, a bound trigger while it is held is the chord and both stay back, another button lets the modifier out first, a lone modifier released under the hold delay is a tap and after it nothing; a modifier held alone for the hold delay is reported once as a long hold, and its end once (`tick`); used by the shell's controls and the game's virtual pad alike |
| `src/gamepad/forward_map.*` | `ForwardMap`: what each pad button and game chord sends to the virtual pad (`padButtons`), as the pads take it |
| `src/gamepad/evdev_device.*` | An evdev node: capabilities, grab, state, reads; whether it is a pad for a reader (`OwnVirtualPads`: the shell skips openSU's own virtual pads, `opensu-retro` reads them) |
| `src/gamepad/virtual_pad.*` | The uinput Xbox 360 pad a game reads, and its evdev node (`node()`) |
| `src/gamepad/evdev_pads.*` | Linux's `PadSource` (`EvdevPads`): every pad, read always and hot-plugged; claims them (`Claim`: Free, Shell grabbed, Game grabbed with virtual pads and its `ForwardMap`, Away) and blocks the virtual pads while a menu or page is up; keeps bound chords from the game (`setChords`); never opens motion or hidraw nodes; lists the connected pads (`connected()`) and where each pad's sticks stand (`sticks()`), and every event names its pad (`Event::source`); kept virtual pads (`keepVirtualPads`) that exist at rest until let go and that the pads drive first under Game, for a sandbox given their nodes up front |
| `src/gamepad/direction_repeat.*` | Held-direction repeat for pads and keys |
| `src/input/shortcuts.*` | The one chord table: every `Action` (pad buttons, Guide, volume, Quit, and the game actions Close game, Save state, Load state, Reset game, Devices, Downloads, Home) with its default key combos and pad chords (any button may be the held one), the player's overrides (`rebind`, with conflict refusals), labels and settings spellings, the chords on one modifier (`chordsOn`), each button's glyph key (`glyphOf`, `buttonOfGlyph`) |
| `src/input/key_names.*` | The shell's key codes (`input::keys`, GLFW's numbering, letters as ASCII) and their names: `Combo`, modifiers, bindable keys, X11 key names for the in-game watcher |
| `src/input/pad_chords.*` | Pad chords in the shell's controls: the `ChordSet`'s triggers, one `ChordGate` per source, the action each chord plays, and the modifier long held now (`longHeld`) |
| `src/input/chord_set.*` | The chords in force: the shell's and the running game's (a chord both bind is the shell's), the hold delay (`holdDelay`, 500 ms), the action a chord plays (`actionFor`) and the chords on one modifier for the chord list (`on`) |
| `src/input/button_map.*` | `ButtonMap`: what each pad button sends and the game chords (pad buttons or an action), refusals (Guide is openSU's), names (`describe`), and the pads' `ForwardMap` made from it (`forwardMap`) |
| `src/input/mapping_scopes.*` | The mapping scopes on the setting layers' `Layer` (game, emulator, system, global; the whole map of the nearest applies) and the Steam Input switch (global, per game); `resolve` picks a game's map, none while Steam Input has it |
| `src/input/keyboard_style.*` | The on-screen keyboard style the player chose (daisy wheel, regular), its settings spelling and label |
| `src/input/prompts.hpp` | `Prompts`: the last device and the shortcuts, so key caps in hints follow remaps |
| `src/input/keyboard_bindings.*` | `KeyFrame` (one frame's key downs, ups, repeats and typed text), `KeySource`, the frame's pressed `Combo`, and typed text; the keys themselves come from `Shortcuts` |
| `src/input/last_device.*` | Which device (pad, or keyboard and mouse) gave the latest real input; read by the prompt painters |

## Built-in libretro runner (`opensu-retro`)

Design: `docs/design/libretro.md`. `opensu_retro` and the `opensu-retro` executable, built beside
`opensu`.

| Path | Owns |
| --- | --- |
| `cmake/Libretro.cmake` | `libretro.h` and `libretro_vulkan.h` from libretro-common at a pinned commit, SHA-256 checked (`opensu::libretro_header`) |
| `cmake/Librashader.cmake` | librashader from the `SomeoneIsWorking/librashader` fork at a pinned commit: `librashader.h` (`opensu::librashader_header`, SHA-256 checked), `librashader-capi` built by cargo (Rust 1.88+, refusal otherwise; `OPENSU_BUILD_LIBRASHADER`), staged and installed as `<libdir>/opensu/librashader.so` |
| `src/retro/main.cpp` | The command line; the default saves (`Config::savesDir`) and system (`Config::retroSystemDir`) folders |
| `src/retro/runner_app.*` | Composition and the frame loop: pacing on a clock at the core's rate, input, sound, save checks, control commands between frames, `--hidden` |
| `src/retro/control_server.*` | The runner's loopback control channel: `{"port", "pid"}` (or `{"error"}` on a load failure) in the `OPENSU_RETRO_CONTROL` file, `/status`, `/state/save`, `/state/load`, `/reset`, `/video` (live video settings), `/frame.png` (hidden runs), each run on the frame loop; `/input`, the control pad's buttons and sticks (`ControlInput`) taken by the loop |
| `src/retro/save_state.*` | The state slot `<saves>/<rom stem>.state`: serialize, atomic write, unserialize |
| `src/retro/shared_library.*` | A library loaded at run time by path through `SDL_LoadObject` and its functions (`SharedLibrary`), for cores and librashader on every OS |
| `src/retro/core.*` | One core: loaded as a `SharedLibrary`, `retro_*` symbols, init/load/run/unload, the context-free callback trampolines (one file-scope pointer), input queries (`InputQuery`) routed by device |
| `src/retro/core_environment.*` | The environment commands answered, the hardware-render ones through `HardwareRender`; everything else refused |
| `src/retro/core_options.*` | Core option definitions (v0, v1, v2, intl), their defaults and the launch's chosen values (`--option`, `OptionValues`) where the core offers them |
| `src/retro/hardware_render.*` | GPU cores: `SET_HW_RENDER` (Vulkan; OpenGL, GLES, Direct3D refused by name), preferred context, context negotiation interface, `retro_hw_render_interface_vulkan` (set_image, semaphores, queue lock, sync index), context reset and destroy, the `RETRO_HW_FRAME_BUFFER_VALID` frame's image and dupes |
| `src/retro/video_frame.*` | 0RGB1555, XRGB8888, RGB565 to RGBA; duplicate frames |
| `src/retro/audio_sink.*` | Resampling ring with ±0.5 % dynamic rate control |
| `src/retro/audio_output.*` | The sink on the SDL playback device, or drained by the clock when hidden |
| `src/retro/joypad.*` | `gamepad::Button` and SDL scancodes to `RETRO_DEVICE_JOYPAD`; the pads' and the control pad's sticks to `RETRO_DEVICE_ANALOG` (analogue stick, touch cursor) |
| `src/retro/save_memory.*` | Save RAM: load, change check, atomic `.srm` write |
| `src/retro/presenter.*` | The SDL window and its events; Vulkan swapchain or hidden offscreen image, each frame's submission (a GPU core's semaphores and command buffers, `CoreSync`) and present, read-back, the wait on the last frame |
| `src/retro/frame_compositor.*` | What a target shows: render pass, frame texture upload or a GPU core's own image sampled in place (`CoreImage`, queue family ownership), placement for aspect and integer scale / overscale (`placeFrame`, `letterbox`), nearest / bilinear / sharp-bilinear sampling; a shader preset's chain drawn into the placed rectangle instead, from the texture or the core's image (copied into the texture when the core keeps it in `GENERAL`); `change`: new video settings between frames, the new preset built first |
| `src/retro/librashader_library.*` | librashader loaded at run time by explicit path (`SharedLibrary`), ABI/API check, its C API calls with errors as `ShaderError`; missing library refusal |
| `src/retro/shader_chain.*` | A slang shader preset (`.slangp`) as a librashader Vulkan filter chain: built once, recorded per frame from the frame image into the viewport; history cleared on the first frame |
| `src/retro/frame_filter.*` | Built-in video filters (none, scanlines, LCD grid, CRT / crt-lottes): which fragment shader a sampling and filter draw with, the filters' parameters as constants, the push-constant block per draw |
| `src/retro/vulkan_device.*` | Vulkan instance (optional validation), device and graphics queue, made through a GPU core's context negotiation (create_instance, create_device2, create_device) when it has one; the queue lock for submit, present and device wait; memory, image views and 2D image descriptions for its owners |
| `src/retro/shaders/` | GLSL for the frame (nearest/bilinear, sharp bilinear, the scanline and LCD-grid bands on the source pixel grid in `frame_grid.glsl`, crt-lottes), compiled by glslc into SPIR-V headers in the build tree (gpu2d's `gpu2d_compile_shaders`) |
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
| `src/ui/rail_painter.*` | XMB/Carousel chrome: the section icon (`library.svg` with its colours swapped for the focused console's border), the header card, the markers and the shadowed title |
| `src/ui/card_border.*` | Reads a console card's border colours, which the section icon is drawn in |
| `src/ui/dock_metrics.*` | The dock capsule's sizes and rectangles in dp (`gh3.i1/j1`, `jj2`); which item a pointer is on (`dockItemAt`) |
| `src/ui/dock_motion.*` | The dock's show/hide (`dockPinned`: Home always, Library by the pin option; else 1200 ms after L1/R1 on Library; show 170 ms ease-out from below, hide 125 ms ease-in straight down) and the icon pop |
| `src/ui/mode_chooser.*` | The options panel's state, focus row and scrolling layout (cards, icon size slider, pin, sort, source, filters, search); the card, row or slider level under a point |
| `src/ui/icon_size.*` | iiSU's icon size levels 1 to 20 and their scale formulas; shared by `home_layout` and `rail_layout` |
| `src/ui/on_screen_keyboard.*` | The one on-screen keyboard (`docs/design/on-screen-keyboard.md`): the line and its cursor, its kind (search, path, password: length, secret wiped on close), the style it opened in, the regular grid's keys and D-pad walk, the daisy wheel's petals, sets and characters (`daisyCharacter`, `petalPointedAt`), the left stick's held directions, the layout of either (`layoutKeyboard`, room reserved under the field) and its hit tests |
| `src/ui/search_panel.*` | Global Search state: an `OnScreenKeyboard`, the results list and its zone, the results' layout in the keyboard's reserved room and their hit test |
| `src/ui/details_page.*` | The details page: what it says about a game (`detailsFor`, `lastPlayedText`; an arcade game's Romset and Nearest rows; `GameSettingsView`: setting rows and the Game settings button), its buttons, geometry (the column beside the buttons the rows go to when they do not fit above them; buttons shrink to stay below the title and badge), focus, the armed button (its label and what it does, let go when focus moves) |
| `src/ui/breadcrumbs.*` | The breadcrumb trail's levels and geometry: cells, chevrons, fitting to the room, the level under a point |
| `src/ui/context_menu.*` | The tile context menu: entries per tile (`contextItemsFor`), focus, the armed entry (let go when focus moves), layout and hit-tests |
| `src/ui/panel_fade.*` | The fade and scale a panel shows and hides with |
| `src/ui/tile_geometry.*` | One tile's rectangles and radii (and `outerForContent`, the inverse of the frame inset); where a game tile's store icons and update badge sit |
| `src/ui/top_bar_metrics.*` | Top bar sizes in dp (`is7`, `hs7`, `dl3`) |
| `src/ui/top_bar_layout.*` | The status pill and the launcher badge cells in it, in pixels; the launcher under a point. The Hud paints and hit-tests from it |
| `src/ui/corner_hints.*` | Which prompts the bottom corners name, from what each button does now (`HintContext`) |
| `src/ui/clock_text.*`, `battery_icon.*` | Clock string and battery drawable choice |
| `src/ui/guide_menu.*` | The Guide menu's entries per context (home or running game), the power button below the rows, the power list (Switch to session mode or Install session mode outside the login session, Switch to desktop inside it), the armed entry, focus; its panel, row and power button rectangles |
| `src/ui/session_dialog.*` | The consent dialog model: title, paragraphs, Accept and Cancel focus, the progress line while a change runs; its layout and hit-test |
| `src/ui/quick_menu.*` | The quick menu's rows (the Settings rows), focus, panel layout and hit-tests |
| `src/ui/settings_tabs.*` | The tabbed side panel (game settings tabs): tabs sized by their labels, rows with an optional Override checkbox, focus on the checkbox or the value, layout and hit-tests |
| `src/ui/panel_frame.hpp` | The frame and chrome heights the side menus are laid out in |
| `src/ui/page_panel.*` | A settings-style page with its fade, as the Settings screen and Devices page use it |
| `src/ui/downloads_page.*`, `downloads_page_painter.*`, `downloads_panel.*` | The Downloads page: rows with their buttons, focus kept by key across refreshes, layout and hit-tests; its painter; its fade and pointer targets |
| `src/ui/tile.hpp`, `tile_artwork.*` | The tile and when it shows the loading placeholder; the artwork decode, tickets, resident textures and pending count (`/state` `artworkPending`) of the shelf |
| `src/ui/launch_panel.*` | The launch/install card's state; its card and hint rectangles from measured widths, and the hint under a point |
| `src/ui/pointer_target.hpp` | What a pointer can be on (session dialog button, breadcrumb level, details button, dock item, tile, page control, layout card, options row, icon size slider, search key or result, context item or backdrop, Guide entry, Guide power button, quick row or slider, settings tab, tab row and its checkbox, page row or slider, download row or button, panel button, launcher badge and whether it shows a download) |

Painters and composition (`opensu_ui`):

| Path | Owns |
| --- | --- |
| `src/ui/shell.*` | The home screen: tiles, focus, paging, draw order; the scene into its own target so the dock glass blurs what lies behind it (`Canvas::blurBehind`, 8 dp) |
| `src/ui/hud.*` | Chrome around the grid: ground, top bar, corner hints, toast (kept above the dock); grid insets |
| `src/ui/settings_page.*`, `settings_page_painter.*` | The Settings screen: categories, rows (toggle, choice, slider, path, action, info), focus, layout and hit-tests; its painter |
| `src/ui/settings_panels.*` | The Settings screen and what opens over it (folder chooser, the path's `OnScreenKeyboard`): their state, fades and painters as one object |
| `src/ui/folder_chooser.*`, `folder_chooser_painter.*` | The console-friendly folder chooser: model and painter |
| `src/ui/volume_osd.*`, `volume_osd_painter.*` | The volume overlay: model (hold, fade) and painter |
| `src/ui/chord_hint.*`, `chord_hint_painter.*` | The held modifier's chord list from the `ChordSet` (partner glyph and action, or what a game chord sends, per chord, `chordHintRows`), shown while it is long held and faded out after; its centred panel painter |
| `src/ui/status_pill.*`, `glass.*` | The one status pill painter: clock, battery and the launchers inline, on a glass body |
| `src/ui/launcher_badges.*` | Launcher logos with status dots, the starting spinner and the download ring, drawn by the status pill painter; the hover light |
| `src/ui/breadcrumb_painter.*`, `details_page_painter.*` | The breadcrumb pill; the details page (cover, backdrop, title, badge, rows above the buttons or wrapped beside them, buttons with their value fitted after the label) |
| `src/ui/vector_icon.*` | The shipped SVG icons (`assets/icons/`), rasterised at drawn size: launcher logos as white masks, openSU's own Home and Library icons (each with a `_selected` variant) in colour, with paint colours swapped on request (`ColourSwap`) |
| `src/ui/progress_spinner.*` | Material's indeterminate circular spinner (a starting launcher's ring) |
| `src/ui/button_glyph.*` | Controller button glyphs (`input_glyph_*`, LB/RB included; LT/RT, d-pad arrows and named caps such as L3 and GUIDE are openSU's), or the bound key's cap when keyboard and mouse were last used |
| `src/ui/dock_painter.*` | The dock capsule: glass, the sections' icons (from `IconAtlas`, rasterised at their resting size), LB/RB badges; the dock's top as drawn (`top`) |
| `src/ui/mode_chooser_painter.*` | The options panel: the cards after iiSU's chooser, with sketched previews, then the slider and switch rows |
| `src/ui/on_screen_keyboard_painter.*` | The on-screen keyboard drawn: card, field with the caret at the cursor (dots for a password), the grid's keys or the daisy wheel (petals, the picked one filled, the face legend, sets between LT and RT, LB/RB), hints; what a caller adds (`KeyboardExtras`) |
| `src/ui/search_panel_painter.*`, `context_menu_painter.*` | The search panel (its results drawn into the keyboard's reserved room) and the tile context menu |
| `src/ui/hint_row.*` | A row of button prompts with labels, centred in a band (`paintHintRow`), for the context menu and the mapping page |
| `src/ui/mapping_page.*`, `mapping_page_painter.*` | The mapping page: title, note, its items by id (pad buttons, chords, Add chord, Steam Input, Reset), the picker's tiles, the listening prompt, spatial focus; its painter (controller board with callouts, strip, picker popover, listening prompt) |
| `src/ui/mapping_board_layout.*` | The mapping page's geometry at design size, scaled to the frame: each pad button's chip and callout on the controller board, the strip (chords scrolled to focus, Steam Input and Reset kept right), the picker's tile lines by group and its popover beside the focused chip inside the card |
| `src/ui/spatial_focus.*` | Which box a direction press moves to among freely placed boxes (nearest within a cone, straight ahead before diagonal); the mapping board and its picker |
| `src/ui/guide_menu_painter.*`, `quick_menu_painter.*` | The Guide menu on the left and the quick menu on the right, over the home screen or a running game |
| `src/ui/side_panel_painter.*` | The right-edge panel's chrome for the quick menu and the settings tabs: scrim, opaque ground, heading, button hints |
| `src/ui/settings_tabs_painter.*` | The settings tabs: tab strip with LB/RB, rows (`SettingsPagePainter::paintTabRow`: checkbox, inherited value dimmed), hints |
| `src/ui/guide_panels.*` | The Guide menu, quick menu and settings tabs with their painters and the one hit test |
| `src/ui/session_dialog_painter.*`, `session_panels.*` | The dialog's painter (wrapped paragraphs, pill buttons with pad glyphs); the dialog, the password keyboard (an `OnScreenKeyboard` of kind Password: dots, Caps on the grid, wiped on close) and the first-run integration chooser with their fades and one hit test |
| `src/ui/integration_chooser.*`, `integration_chooser_painter.*` | The first-run integration chooser: its rows, focus and switches, its card layout (shrinks to fit) and painter |
| `src/ui/tile_painter.*` | One tile: shadow, ring, chrome, art (a spinner while it is on its way), platform frame, store icons, the update badge; the name cards of a console, a launcher and All games |
| `src/ui/page_pill.*`, `page_arrow.*` | WiiSu page dots and page arrows |
| `src/ui/paint.hpp` | What the painters draw with: gpu2d's `Canvas`, `Colour` and `Point`, the layout's `Rect` on the canvas, corner radii, and the `ScopedClip` and `PanelScope` (scale about a point) scopes |
| `src/ui/round_shape.*` | Tessellated rounded shapes with per-vertex colour |
| `src/ui/platform.*`, `platform_stroke.hpp` | Console border sprites, logos, stroke colours |
| `src/ui/image_decoder.*` | Cover decode and downscale on a worker thread; the frame only uploads what it takes |
| `src/ui/glyph_textures.*` | The console glyphs drawn in a ROM tile's frame tab: files by system, textures on load |
| `src/ui/typeface.*` | Text drawing at Android em sizes: the face loaded into gpu2d's text, labels drawn a glyph at a time where `TextGrid` puts them, fitted with an ellipsis, wrapped at spaces (`wrapped`) |
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
| `tools/screenshots/` | The README screenshots and the look-parity references (`docs/design/sdl3-gpu-shell.md` §5): a demo library of freely licensed games (`demo`, art pinned by sha256), openSU run hidden with no network (`offline`, `shell`), a fixed-volume `wpctl` stand-in for parity runs (`mixer`), a `legendary` stand-in for the store scenes' own run, whose install sits at 42% and whose installed game's uninstall waits (`legendary`), the screens driven over the control channel, each reached on a `/state` field and taken once `artworkPending` is 0 (`scenes`, `readme` marks the README set), WebP output (`encode`), `--parity DIR` raw PNGs of every scene, and `compare` (per-scene mean, max and share of differing pixels, difference images, non-zero exit over thresholds); credits in `docs/screenshots/CREDITS.md` |
| `tools/download.py` | The one checksum-verified download the tools share |
| `tools/podman.py` | Container images the tools build, tagged by the hash of their recipe, and the worktree git dir a container must also mount |
| `packaging/ci/Containerfile` | The CI image: Fedora 44 (the fork's podman build release) with openSU's and the fork's build dependencies and systemd as PID 1, so tests get a user manager |
| `packaging/appimage/Containerfile` | The AppImage build image: Ubuntu 24.04 (the glibc 2.39 floor), openSU's and the fork's build dependencies, wayland, wayland-protocols, libxkbcommon, pixman and libdrm headers newer than 24.04's from pinned tarballs, SDL3, SDL3_ttf and SDL3_image built from pinned release tarballs, uv |
| `packaging/appimage/signing-key.asc` | The public half of the key release AppImages are signed with (private half: the `OPENSU_APPIMAGE_SIGNING_KEY` repository secret) |
| `packaging/opensu.desktop.in`, `packaging/opensu.svg` | The application menu entry and openSU's icon, installed to `share/applications` and `share/icons/hicolor/scalable/apps`; the AppImage carries both at its top |
| `.github/workflows/ci.yml` | CI on a release tag (`v*`) or a manual run: `tools.check ci` (`all` in the `packaging/ci` container, fork built natively), and `tools.appimage all` (build and smoke) on Ubuntu 24.04 |
| `.github/workflows/release.yml` | A `v*` tag: the signed AppImage built and smoke-tested, then a GitHub Release with the AppImage, its `.sha256`, `.zsync` and the sign-in `.xpi` |

## Tests and docs

- `tests/<area>/`: one ctest suite per owner, through the shipping code.
- `tests/support/`: shared fakes: `fake_steam.hpp` (a DevTools Steam), `release_server.hpp` (a release API and asset host), `buildbot_server.hpp` (a libretro buildbot nightly folder), `zip_fixture.hpp` (zips built in memory), `loopback_server.hpp`, `fake_runner.hpp` (`opensu-retro`'s control channel); `scratch_dir.hpp`, a test's own empty directory under the test scratch.
- `tests/plugin/`: the plugin host through the shipping runtime (`plugin_rig.hpp`), the shipped Eden plugin against fixture files.
- `docs/design/`: designs (`lua-plugins.md`: the Lua plugin host, phase 1 built, later phases proposed).
- `docs/re-frontier.md`: what is recovered from iiSU. `docs/reference/` (private `opensu-re`
  checkout): the evidence and the spec built on it.
