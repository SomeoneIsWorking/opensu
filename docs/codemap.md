# Code map

Where each concept lives. Read this before placing code; update it in the change that adds or
moves an owner. The UI replicates iiSU, so UI owners cite the iiSU function they reproduce and the
evidence is in `reference/iisu/` (the private `iideck-re` repository, checked out at the
gitignored `docs/reference/`).

## Entry and composition

| Path | Owns |
| --- | --- |
| `src/main.cpp` | Argument parsing; starts the nested session or the shell; `--render FILE` renders one frame headless |
| `src/app/shell_app.*` | Composition: catalog, controller reader, Steam client, launches, the drawn shell, frame loop; plays the UI sounds at the input events that iiSU plays them at |
| `src/app/control_channel.*` | Loopback HTTP control channel (`lucent::http::Server`): shell state, input, frames, `/signin/<store>[/start]` |
| `src/app/sign_in.*` | Store sign-in steps: open the sign-in page in the default browser, finish with the code (GOG token, Epic via `legendary auth --code`) |
| `src/app/launcher_status.*` | The launcher badges' states from the Steam client, its downloads and the catalog's store statuses |
| `src/app/install_job.*` | The store-neutral install job: its thread, the latest report the loop takes, the licence answer |
| `src/app/cli_install_job.*` | An install by a downloader program: run it, turn its logged progress into reports, fail with its reason; the base of the Epic and GOG jobs |
| `src/app/steam_install_job.*`, `epic_install_job.*`, `gog_install_job.*` | One Steam install (walks Steam's installer, follows its queue); one Epic install (`legendary install`); one GOG install (`gogdl download`: token handed over and taken back, Linux or Windows build, the install recorded) |
| `src/app/installs.*` | The installers by store, one install at a time; which stores install |
| `src/config/config.*` | The one reader of the environment, into typed immutable config (cache, data and config dirs included); where the Gamescope fork binary is (`gamescopeBeside`, relative to `/proc/self/exe`) |
| `src/settings/settings.*` | The player's saved preferences (Library layout mode) as JSON under the config dir; defaults on a missing or corrupt file |
| `src/fileio/atomic_write.*` | Whole-file writes through a `.part` file and a rename |
| `extension/iideck-signin/` | Firefox/Zen WebExtension that hands a GOG or Epic sign-in code to the control channel |

## Session and processes (G003, G004)

| Path | Owns |
| --- | --- |
| `src/session/nested_session.*` | Re-running iideck inside its own Gamescope when not already in one |
| `src/session/gamescope.*` | The Gamescope command line (`--close-focused-window` included) |
| `cmake/Gamescope.cmake` | Building the pinned Gamescope fork in podman (commit, container steps, staging and install path); `IIDECK_BUILD_GAMESCOPE`. `cmake/GamescopeImage.cmake` builds the image if absent, `cmake/GamescopeLddCheck.cmake` checks the staged binary's libraries |
| `packaging/gamescope-build/Containerfile` | The Gamescope build image: host's Fedora release plus Gamescope's build dependencies |
| `src/session/monitor.*` | The output's size and refresh |
| `src/session/gamescope_overlay.*` | iideck's window as Gamescope's overlay over a running game |
| `src/session/game_keys.*` | Keyboard shortcuts while a game has the keyboard (Shift+Tab is Guide), from XInput2 raw keys |
| `src/session/gamescope_windows.*` | Which processes own a window Gamescope would show; tells the handoff when a game is on screen |
| `src/launch/instance.*` | One transient systemd user scope per launch; stopping it ends the whole tree |
| `src/launch/handoff.*` | Starting a game, reporting its progress until it shows a window, hiding the shell until it ends |
| `src/launch/process_tree.*` | Finding processes by command line, ending trees |
| `src/launch/command.*`, `argv.hpp` | Short-lived children, a child streamed line by line or its stdout captured (own `iideck_command` target, so `library` can run `legendary`), executable lookup, exec argv |
| `src/launch/steam_gate.hpp` | What a Steam launch waits for from the owned Steam client |
| `src/launch/launch_progress.*` | A launch's stage before its window (waiting for Steam, updating, starting, loading) and its line of text |
| `src/launch/game_windows.hpp` | What the handoff asks the display: does the game show a window yet |
| `src/steam/client.*` | The Steam client iideck owns: start in background, readiness, state |
| `src/steam/desktop_steam.*` | Detecting a Steam client running outside iideck |
| `src/steam/devtools.*` | Running JavaScript in Steam's SharedJSContext over Chrome DevTools |
| `src/steam/downloads.*` | Steam's live download queue: parsing and reading it |
| `src/steam/launch_activity.*` | Steam's game actions and running state per app, recorded in its SharedJSContext |
| `src/steam/install_wizard.*` | Installing a Steam app through Steam's own installer, licence steps to the player |

## Library

| Path | Owns |
| --- | --- |
| `src/library/game.*` | The launchable `Game` record |
| `src/library/catalog.*` | Building the catalog from all sources |
| `src/library/steam.*`, `epic.*`, `gog.*`, `roms.*` | One source each |
| `src/library/gog_auth.*`, `gog_token.*` | GOG's OAuth sign-in and the saved token (`<data dir>/gog-token.json`); `writeOwnerOnly`, the one owner-only atomic write |
| `src/library/gogdl_auth.*` | The token as gogdl's `--auth-config-path` file, written for an install and read back |
| `src/library/gog_installs.*` | Where GOG files live under the data dir (`Paths`) and the record of finished GOG installs |
| `src/library/install_log.*` | What legendary and gogdl log while installing: progress fraction, ERROR/CRITICAL lines |
| `src/library/sections.*` | The dock's sections (Home, Library), the active one and L1/R1 cycling with wrap; Library's layout modes and their keys |
| `src/library/shelf.*` | What the grid holds: Home's installed store games (`homeShelf`); Library's launchers, All games and consoles (`libraryShelf`); a console's ROMs, a launcher's library, the combined library; moving between them and between sections |
| `src/library/titles.*` | The same title across stores: the comparison key, merged copies, preference order |
| `src/library/rom_systems.*` | Known systems: folder names, game files, the file a game folder starts; the tables are `constexpr` (`name_list.hpp`) |
| `src/library/emulators.*` | Which emulator runs each system here, and its command line |
| `src/artwork/libretro_index.*` | Matching a ROM's name to libretro-thumbnails' box art listing |
| `src/artwork/artwork_store.*` | Downloaded artwork on disk under the cache dir: paths, misses, listings, frame glyphs, the starter pack file, UI sounds (`sound/<file>`, WAV or OGG) and the dock's icons (`nav/`) |
| `src/artwork/artwork_fetcher.*` | The background downloads: Steam's CDN for Steam, gamesdb (else the library tile) for GOG, the key image URL for Epic, libretro-thumbnails for ROMs, iiSU's starter pack for console cards, iiSU's border pack for frame glyphs, the APK's sounds and nav drawables (`ApkAsset`) |
| `src/artwork/apk_archive.*` | iiSU's release APK read by HTTP ranges: central directory once, any entry by range with its CRC checked; `fetch` is find and extract with a found/missing/failed result (glyphs, sounds and nav icons use it) |
| `src/artwork/starter_pack.*` | iiSU's starter pack: its entry taken out of the APK against a pin, and a system's card as PNG |
| `src/artwork/console_glyphs.*` | iiSU's frame glyphs: `border_pack.json` read once, a system's `logo_*.png` out of the APK |
| `src/artwork/iisu_assets.*` | Which APK entry is each UI sound and each dock icon (the nav table is valid for the pinned APK only) |
| `src/artwork/zip_archive.*` | The one zip reader: end record, central directory, local header offset, checked extraction; an in-memory archive |
| `src/audio/effect.*`, `debounce.*` | iiSU's UI sounds iideck plays (`yp8`): effect to APK file name, the Domino cues and `dominoFor(tileCount)` (`xp8.a`), the 91 ms repeat rule for Enter/ExitConsolesApps and the Domino cues; pure (`iideck_audio_model`) |
| `src/audio/sound_player.*` | raylib audio: the device opened once (silent with one warning when absent), the WAVs loaded from the store, `play` through the debounce |
| `src/net/web_client.*` | HTTPS GETs over libcurl, with headers; shared by artwork and the stores |
| `src/vdf/` | Valve KeyValues parser |
| `src/device/battery.*` | Battery level and charging state from sysfs |
| `src/gamepad/event.*` | The shell's controls (`Button`, `Event`), raylib-free (`iideck_pad`) |
| `src/gamepad/pad_translator.*` | One physical pad's evdev events as the virtual pad's and as controls |
| `src/gamepad/evdev_device.*` | An evdev node: capabilities, grab, state, reads |
| `src/gamepad/virtual_pad.*` | The uinput Xbox 360 pad a game reads |
| `src/gamepad/pads.*` | Every pad, read always and hot-plugged; holds them during a game and blocks them while the Guide menu is open |
| `src/gamepad/direction_repeat.*` | Held-direction repeat for pads and keys |
| `src/input/keyboard_bindings.*` | The one key table (raylib key to shell button), the key cap a button's prompt shows, glyph key to button |
| `src/input/last_device.*` | Which device (pad, or keyboard and mouse) gave the latest real input; read by the prompt painters |

## Home UI (G002)

Pure model, unit-tested without raylib (`iideck_grid`, `iideck_hud_model`):

| Path | Owns |
| --- | --- |
| `src/ui/home_layout.*` | Grid geometry for Standard and WiiSu: cells, gaps, insets, placeholder slots, scrolling, page pill and page arrow rects (`hx2.g`, `zj2`, `ou4.q`, `ys8.h/k/l`) |
| `src/ui/grid_focus.*` | D-pad focus movement and page crossing (`hx2.z/O`) |
| `src/ui/tile_motion.*` | Focus scale, domino entrance, press pulse, ring rotation, FastOutSlowIn, the rail's visual-index easing |
| `src/ui/section_view.*` | Per section: the grid viewport (3 rows x 4 columns, column-major, horizontal paging, both sections) and the presentation (Grid, XMB, Carousel) |
| `src/ui/rail_layout.*` | XMB and Carousel geometry (navigation.md 5.3): tile rectangles from the fractional focus, left column, header card, marker and title anchors |
| `src/ui/rail_painter.*` | XMB/Carousel chrome: the recoloured section icon, the header card, the markers and the shadowed title |
| `src/ui/icon_recolour.*` | Reads a console card's border colours and recolours the section icon with them |
| `src/ui/backdrop_blur.*` | The dock glass's 8 dp backdrop blur: scene texture, separable Gaussian, capsule mask |
| `src/ui/dock_metrics.*` | The dock capsule's sizes and rectangles in dp (`gh3.i1/j1`, `jj2`); which item a pointer is on (`dockItemAt`) |
| `src/ui/dock_motion.*` | The dock's show/hide (pinned on Home, 1200 ms after L1/R1 on Library; show 170 ms ease-out from below, hide 125 ms ease-in straight down) and the icon pop |
| `src/ui/mode_chooser.*` | The Library layout picker's open state, focus and card/panel layout |
| `src/ui/tile_geometry.*` | One tile's rectangles and radii (and `outerForContent`, the inverse of the frame inset); where a game tile's store icons sit |
| `src/ui/top_bar_metrics.*` | Top bar sizes in dp (`is7`, `hs7`, `dl3`) |
| `src/ui/clock_text.*`, `battery_icon.*` | Clock string and battery drawable choice |
| `src/ui/game_menu.*` | The Guide menu's items and focus (iideck's own) |

Painters and composition (`iideck_ui`):

| Path | Owns |
| --- | --- |
| `src/ui/shell.*` | The home screen: tiles, focus, paging, draw order |
| `src/ui/hud.*` | Chrome around the grid: ground, top bar, corner hints, toast; grid insets |
| `src/ui/status_pill.*`, `glass.*` | Status pill and its glass body |
| `src/ui/launcher_badges.*` | Launcher logos with status dots and the download ring, in the top bar's friends slot |
| `src/ui/vector_icon.*` | The shipped SVG icons (`assets/icons/`), rasterised at drawn size |
| `src/ui/progress_spinner.*` | Material's indeterminate circular spinner (the bell's busy ring) |
| `src/ui/button_glyph.*` | Controller button glyphs (`input_glyph_*`, LB/RB included), or the bound key's cap when keyboard and mouse were last used |
| `src/ui/dock_painter.*` | The dock capsule: glass, nav icons, LB/RB badges |
| `src/ui/mode_chooser_painter.*` | The Library layout picker: the cards page after iiSU's chooser, with sketched previews |
| `src/ui/game_menu_painter.*` | The Guide menu over a running game |
| `src/ui/tile_painter.*` | One tile: shadow, ring, chrome, art, platform frame, store icons; the name cards of a console, a launcher and All games |
| `src/ui/page_pill.*`, `page_arrow.*` | WiiSu page dots and page arrows |
| `src/ui/round_shape.*` | Tessellated rounded shapes with per-vertex colour |
| `src/ui/platform.*`, `platform_stroke.cpp` | Console border sprites, logos, stroke colours |
| `src/ui/glyph_textures.*` | The console glyphs drawn in a ROM tile's frame tab: files by system, textures on load |
| `src/ui/typeface.*`, `face_metrics.*` | Text drawing at Android em sizes; the face's line metrics |

## Tests and docs

- `tests/<area>/`: one ctest suite per owner, through the shipping code.
- `docs/re-frontier.md`: what is recovered from iiSU. `docs/reference/` (private `iideck-re`
  checkout): the evidence and the spec built on it.
