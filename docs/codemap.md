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
| `src/app/shell_app.*` | Composition: catalog, controller reader, Steam client, launches, the drawn shell, frame loop; plays the UI sounds at the input events that iiSU plays them at |
| `src/app/control_bridge.*` | The hand-over between the control channel's threads and the loop: queued input, reload and close requests, the published `ShellSnapshot`, the frame handshake |
| `src/app/game_screen.*` | The window's relation to a running game: the Gamescope overlay and game-time key watcher, or the hidden window flag; shows the window only while a menu or page is up |
| `src/app/guide_menu_controller.*` | The Guide menu's buttons and what each entry does (sections, Devices, Settings, close game, quit, the power button and list: `host::Power` with a second press for restart and shut down, `host::SessionMode` for Switch to session mode (second press), Install session mode (opens the dialog) and, in a login session, Switch to desktop) |
| `src/app/session_install_controller.*` | The Install session mode flow: the dialog, then the password on the on-screen or physical keyboard, the root step on a worker, a wrong password asking again, the password wiped once sudo has had it |
| `src/app/keyboard_walk.*` | A pad button on the on-screen keyboard for the entries that type a line (folder path, password): the one implementation `path_editor` and the session install share |
| `src/app/quick_menu_controller.*` | The quick menu's rows and buttons: volume, mute, output, brightness, Bluetooth, controller batteries, Devices, Close game |
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
| `src/app/details_controller.*` | The details page's buttons: open for the focused game, Play or Install, the emulator pick, Hide, Back; keeps the page current as the library changes |
| `src/app/breadcrumb_trail.*` | The one computation of the breadcrumb trail from the shell's state (section, folder, search, filters, open game) |
| `src/app/settings_controller.*` | The Settings screen's controller: rows per category, input routing, saving through `Preferences`, folder chooser and path entry |
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
| `src/app/installs.*` | The installers by store, one install at a time; which stores install |
| `src/config/config.*` | The one reader of the environment, into typed immutable config (cache, data and config dirs included, the logind session id); where the Gamescope fork binary is (`gamescopeBeside`, relative to `/proc/self/exe`) |
| `src/settings/settings.*` | The player's saved preferences (Library layout, dock pin, icon size, sort and filters, hidden games, last-played times, Settings-screen choices, interface scale, install folders, shortcut remaps) as JSON under the config dir; defaults on a missing or corrupt file |
| `src/settings/install_folders.*` | The one resolver of where a store installs: a store's override, else the default, else none (the store's own choice); validation (exists, writable) |
| `src/fileio/atomic_write.*` | Whole-file writes through a `.part` file and a rename |
| `extension/opensu-signin/`, `extension/CMakeLists.txt` | Firefox/Zen WebExtension that hands a GOG or Epic sign-in code to the control channel; packed into `opensu-signin.xpi` and installed beside the assets |

## Session and processes (G003, G004)

| Path | Owns |
| --- | --- |
| `src/session/compositor_session.*` | Re-running openSU inside its own Gamescope when not already in one: nested at the monitor's size on a desktop, top level on DRM at the native mode for `--session` |
| `src/host/session_mode.*` | Session mode, the one owner of switching sessions: `installed()` (an entry for this openSU in a directory SDDM reads, and the selector), the root step (`sudo -S -k` with the password on stdin, one `install-session.sh`), Switch to session mode (selector `opensu`, then KDE's `org.kde.Shutdown.logout`) and Switch to desktop (selector `restore`, then stopping the Gamescope scope); read-only wrapper for hidden runs |
| `src/host/login_session_end.*`, `src/session/login_session.*` | What the next login is when the login session ends: an abnormal end (Gamescope non-zero, by a signal, or within 30 s) runs `sudo -n <selector> restore` so SDDM autologin cannot relogin in a loop; a logout or shutdown signal and a deliberate Switch to desktop (the note) leave it alone; `LoginSession` composes the top-level `CompositorSession` with the judgement and the desktop fallback |
| `src/host/desktop_request.*` | The note Switch to desktop leaves in the runtime dir (it marks the end deliberate), and, as the fallback when the selector is not installed, `startplasma-wayland` run in the login session's place once Gamescope has ended |
| `packaging/install-session.sh`, `packaging/opensu-session-select.sh` | The two root-run POSIX sh scripts, installed to `<prefix>/libexec/opensu/`: the one-sudo installer (session entry, `/usr/local/libexec/opensu-session-select`, `/etc/sudoers.d/zz-opensu-session-select`) and the selector (`opensu` or `restore`: SDDM drop-in `zz-opensu-session.conf` or the last session in `state.conf`) |
| `src/session/gamescope.*` | The Gamescope command line, nested (`--close-focused-window` included) or top level (`--backend drm`) from `Output::native()` |
| `packaging/opensu-session.desktop.in` | The Wayland session entry, installed to `<prefix>/share/wayland-sessions/opensu.desktop` (`install-session.sh` copies it where SDDM reads) |
| `cmake/Gamescope.cmake` | Building the pinned Gamescope fork in podman (commit, container steps, staging and install path); `OPENSU_BUILD_GAMESCOPE`. `cmake/GamescopeImage.cmake` builds the image if absent, `cmake/GamescopeLddCheck.cmake` checks the staged binary's libraries |
| `packaging/gamescope-build/Containerfile` | The Gamescope build image: host's Fedora release plus Gamescope's build dependencies |
| `src/session/monitor.*` | The output's size and refresh |
| `src/session/gamescope_overlay.*` | openSU's window as Gamescope's overlay over a running game |
| `src/session/game_keys.*` | The shortcuts that work while a game has the keyboard (Guide, volume), matched against the `Shortcuts` table from XInput2 raw keys |
| `src/session/gamescope_windows.*` | Which processes own a window Gamescope would show; tells the handoff when a game is on screen |
| `src/launch/instance.*` | One transient systemd user scope per launch; stopping it ends the whole tree |
| `src/launch/handoff.*` | Starting a game, reporting its progress until it shows a window, hiding the shell until it ends |
| `src/launch/process_tree.*` | Finding processes by command line, ending trees |
| `src/launch/command.*`, `argv.hpp` | Short-lived children, a child streamed line by line, its stdout captured or fed one line on stdin (`runCapturedWithLine`, the sudo password) (own `opensu_command` target, so `library` can run `legendary`), executable lookup, exec argv |
| `src/launch/steam_gate.hpp` | What a Steam launch waits for from the owned Steam client |
| `src/launch/launch_progress.*` | A launch's stage before its window (waiting for Steam, updating, starting, loading) and its line of text |
| `src/launch/game_windows.hpp` | What the handoff asks the display: does the game show a window yet |
| `src/steam/client.*` | The Steam client openSU owns: start in background, readiness, state |
| `src/steam/desktop_steam.*` | Detecting a Steam client running outside openSU |
| `src/steam/orphaned_steam.*` | Finding and ending a Steam scope left by an openSU that died |
| `src/steam/devtools.*` | Running JavaScript in Steam's SharedJSContext over Chrome DevTools |
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
| `src/library/emulator_choice.*` | The emulator picked per ROM (`settings.json`): points the game's `launch` and `emulator` at the pick, `unavailable` when it is not installed |
| `src/library/play_history.*` | When openSU last launched each game, for the recent sort of stores that do not say |
| `src/library/titles.*` | The same title across stores: the comparison key, merged copies, preference order |
| `src/library/rom_titles.*` | A ROM's display title from its name: tags, release numbers and the ", The" order (`cleanTitle`); the one title cleanup, also the release-number rule the libretro matcher uses |
| `src/library/arcade_names.*` | Arcade short name to description: the libretro-database `.dat` parser and `NameDb`, the parsed names kept under `<cache>/names/` |
| `src/library/rom_systems.*` | Known systems: folder names, game files, the file a game folder starts; the tables are `constexpr` (`name_list.hpp`) |
| `src/library/emulators.*` | Which emulator runs each system here, and its command line; every emulator of a system, installed or not (`options`), which a ROM's details page lists |
| `src/artwork/libretro_index.*` | Matching a ROM's name to libretro-thumbnails' box art listing |
| `src/artwork/arcade_dat.*` | The pinned libretro-database arcade listings (FinalBurn Neo, MAME 2016): download against size and CRC-32, parse, merge; the fetcher keeps the result in `NameDb` |
| `src/artwork/artwork_store.*` | Downloaded artwork on disk under the cache dir: paths, misses, listings, frame glyphs, the starter pack file, UI sounds (`sound/<file>`, WAV or OGG) and the dock's icons (`nav/`) |
| `src/artwork/artwork_fetcher.*` | The background downloads (queue stages per game, on-screen games first, a settled game reported as not saved): Steam's CDN for Steam, gamesdb (else the library tile) for GOG, the key image URL for Epic, libretro-thumbnails for ROMs, iiSU's starter pack for console cards, iiSU's border pack for frame glyphs, the APK's sounds and nav drawables (`ApkAsset`) |
| `src/artwork/apk_archive.*` | iiSU's release APK read by HTTP ranges: central directory once, any entry by range with its CRC checked; `fetch` is find and extract with a found/missing/failed result (glyphs, sounds and nav icons use it) |
| `src/artwork/starter_pack.*` | iiSU's starter pack: its entry taken out of the APK against a pin, and a system's card as PNG |
| `src/artwork/console_glyphs.*` | iiSU's frame glyphs: `border_pack.json` read once, a system's `logo_*.png` out of the APK |
| `src/artwork/iisu_assets.*` | Which APK entry is each UI sound and each dock icon (the nav table is valid for the pinned APK only) |
| `src/artwork/zip_archive.*` | The one zip reader: end record, central directory, local header offset, checked extraction; an in-memory archive |
| `src/audio/effect.*`, `debounce.*` | iiSU's UI sounds openSU plays (`yp8`): effect to APK file name, the Domino cues and `dominoFor(tileCount)` (`xp8.a`), the 91 ms repeat rule for Enter/ExitConsolesApps and the Domino cues; pure (`opensu_audio_model`) |
| `src/audio/volume_backend.*` | PC output volume through wpctl or pactl (injectable runner), a read-only shadow for hidden runs, the install advice when no backend exists |
| `src/audio/system_volume.*` | Volume and mute state, steps, one listener, async polling of changes made elsewhere |
| `src/audio/sound_player.*` | raylib audio: the device opened once (silent with one warning when absent), the WAVs loaded from the store, `play` through the debounce |
| `src/net/web_client.*` | HTTPS GETs over libcurl, with headers; shared by artwork and the stores |
| `src/vdf/` | Valve KeyValues parser |
| `src/device/battery.*` | Battery level and charging state from sysfs |
| `src/gamepad/event.*` | The shell's controls (`Button`, `Event`), raylib-free (`opensu_pad`) |
| `src/gamepad/pad_translator.*` | One physical pad's evdev events as the virtual pad's and as controls |
| `src/gamepad/evdev_device.*` | An evdev node: capabilities, grab, state, reads |
| `src/gamepad/virtual_pad.*` | The uinput Xbox 360 pad a game reads |
| `src/gamepad/pads.*` | Every pad, read always and hot-plugged; holds them during a game and blocks them while a menu or page is up; lists the connected pads (`connected()`), and every event names its pad (`Event::source`) |
| `src/gamepad/direction_repeat.*` | Held-direction repeat for pads and keys |
| `src/input/shortcuts.*` | The one chord table: every `Action` (pad buttons, Guide, volume, Quit) with its default key combos and pad chords, the player's overrides (`rebind`, with conflict refusals), labels and settings spellings |
| `src/input/key_names.*` | raylib-free key names: `Combo`, modifiers, bindable keys, X11 key names for the in-game watcher |
| `src/input/pad_chords.*` | Pad chords (L2, R2, L3, R3 or Guide plus a button): the held modifier swallows its trigger and emits the action; Guide alone is a tap that comes out on release |
| `src/input/prompts.hpp` | `Prompts`: the last device and the shortcuts, so key caps in hints follow remaps |
| `src/input/keyboard_bindings.*` | `KeySource`, the frame's pressed `Combo`, and typed text; the keys themselves come from `Shortcuts` |
| `src/input/last_device.*` | Which device (pad, or keyboard and mouse) gave the latest real input; read by the prompt painters |

## Home UI (G002)

Pure model, unit-tested without raylib (`opensu_grid`, `opensu_hud_model`):

| Path | Owns |
| --- | --- |
| `src/ui/home_layout.*` | Grid geometry; `visibleSlots`, the one visible-window rule painting, hit-testing and artwork requests share for Standard and WiiSu: cells, gaps, insets, placeholder slots, scrolling, page pill and page arrow rects, and the slot or page control under a point (`hx2.g`, `zj2`, `ou4.q`, `ys8.h/k/l`) |
| `src/ui/grid_focus.*` | D-pad focus movement and page crossing (`hx2.z/O`) |
| `src/ui/tile_motion.*` | Focus scale, domino entrance, press pulse, ring rotation, FastOutSlowIn, the rail's visual-index easing |
| `src/ui/section_view.*` | Per section: the grid viewport (3 rows x 4 columns, column-major, horizontal paging, both sections) and the presentation (Grid, XMB, Carousel) |
| `src/ui/rail_layout.*` | XMB and Carousel geometry (navigation.md 5.3): rectangles of the tiles near the canvas only (`first()`), from the fractional focus, left column, header card, marker and title anchors; the tile under a point (`railTileAt`) |
| `src/ui/rail_painter.*` | XMB/Carousel chrome: the recoloured section icon, the header card, the markers and the shadowed title |
| `src/ui/icon_recolour.*` | Reads a console card's border colours and recolours the section icon with them |
| `src/ui/backdrop_blur.*` | The dock glass's 8 dp backdrop blur: scene texture, separable Gaussian, capsule mask |
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
| `src/ui/tile.hpp`, `tile_artwork.*` | The tile; the artwork decode, tickets and resident textures of the shelf |
| `src/ui/launch_panel.*` | The launch/install card's state; its card and hint rectangles from measured widths, and the hint under a point |
| `src/ui/pointer_target.hpp` | What a pointer can be on (session dialog button, breadcrumb level, details button, dock item, tile, page control, layout card, options row, icon size slider, search key or result, context item or backdrop, Guide entry, Guide power button, quick row or slider, page row or slider, panel button, launcher badge) |

Painters and composition (`opensu_ui`):

| Path | Owns |
| --- | --- |
| `src/ui/shell.*` | The home screen: tiles, focus, paging, draw order |
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
| `src/ui/round_shape.*` | Tessellated rounded shapes with per-vertex colour |
| `src/ui/platform.*`, `platform_stroke.cpp` | Console border sprites, logos, stroke colours |
| `src/ui/image_decoder.*` | Cover decode and downscale on a worker thread; the frame only uploads what it takes |
| `src/ui/glyph_textures.*` | The console glyphs drawn in a ROM tile's frame tab: files by system, textures on load |
| `src/ui/typeface.*`, `face_metrics.*` | Text drawing at Android em sizes; the face's line metrics |

## Maintainer tooling

| Path | Owns |
| --- | --- |
| `tools/vm/` | The headless Fedora 44 KDE guest for login-session tests (`docs/vm-harness.md`): `provision`/`kickstart`/`fetch`/`hostfiles`/`configure` (install and host-like setup), `machine` (qemu command line, PID lifecycle, overlays), `qmp` and `vnc` (screenshots, input), `guest` (SSH), `deploy` (build, install, ship), `control` (openSU's control channel in the guest), `cli` |
| `pyproject.toml`, `uv.lock` | The one locked Python environment of the tools (`uv run --frozen`) |

## Tests and docs

- `tests/<area>/`: one ctest suite per owner, through the shipping code.
- `docs/re-frontier.md`: what is recovered from iiSU. `docs/reference/` (private `opensu-re`
  checkout): the evidence and the spec built on it.
