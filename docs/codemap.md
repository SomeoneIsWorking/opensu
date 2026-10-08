# Code map

Where each concept lives. Read this before placing code; update it in the change that adds or
moves an owner. The UI replicates iiSU, so UI owners cite the iiSU function they reproduce and the
evidence is in `reference/iisu/` (the private `iideck-re` repository, checked out at the
gitignored `docs/reference/`).

## Entry and composition

| Path | Owns |
| --- | --- |
| `src/main.cpp` | Argument parsing; starts the nested session or the shell; `--render FILE` renders one frame headless |
| `src/app/shell_app.*` | Composition: catalog, controller reader, Steam client, launches, the drawn shell, frame loop |
| `src/app/control_channel.*` | Loopback HTTP control channel (`lucent::http::Server`): shell state, input, frames, `/signin/<store>[/start]` |
| `src/app/sign_in.*` | Store sign-in steps: open the sign-in page in the default browser, finish with the code (GOG token, Epic via `legendary auth --code`) |
| `src/app/launcher_status.*` | The launcher badges' states from the Steam client, its downloads and the catalog's store statuses |
| `src/app/install_job.*` | One Steam install: walks the installer, waits on the player's licence answer, follows the download |
| `src/config/config.*` | The one reader of the environment, into typed immutable config; where the Gamescope fork binary is (`gamescopeBeside`, relative to `/proc/self/exe`) |
| `extension/iideck-signin/` | Firefox/Zen WebExtension that hands a GOG or Epic sign-in code to the control channel |

## Session and processes (G003, G004)

| Path | Owns |
| --- | --- |
| `src/session/nested_session.*` | Re-running iideck inside its own Gamescope when not already in one |
| `src/session/gamescope.*` | The Gamescope command line (`--close-focused-window` included) |
| `cmake/Gamescope.cmake` | Building the pinned Gamescope fork (commit, dependency check, staging and install path); `IIDECK_BUILD_GAMESCOPE` |
| `src/session/monitor.*` | The output's size and refresh |
| `src/session/gamescope_overlay.*` | iideck's window as Gamescope's overlay over a running game |
| `src/session/game_keys.*` | Keyboard shortcuts while a game has the keyboard (Shift+Tab is Guide), from XInput2 raw keys |
| `src/session/gamescope_windows.*` | Which processes own a window Gamescope would show; tells the handoff when a game is on screen |
| `src/launch/instance.*` | One transient systemd user scope per launch; stopping it ends the whole tree |
| `src/launch/handoff.*` | Starting a game, reporting its progress until it shows a window, hiding the shell until it ends |
| `src/launch/process_tree.*` | Finding processes by command line, ending trees |
| `src/launch/command.*`, `argv.hpp` | Short-lived children, executable lookup, exec argv |
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
| `src/library/gog_auth.*`, `gog_token.*` | GOG's OAuth sign-in and the saved token (`<data dir>/gog-token.json`) |
| `src/library/shelf.*` | What the grid holds: Home's consoles and store games, a console's ROMs, and moving between them |
| `src/library/rom_systems.*` | Known systems: folder names, game files, the file a game folder starts |
| `src/library/emulators.*` | Which emulator runs each system here, and its command line |
| `src/artwork/libretro_index.*` | Matching a ROM's name to libretro-thumbnails' box art listing |
| `src/artwork/artwork_store.*` | Downloaded artwork on disk under the cache dir: paths, misses, listings, frame glyphs, the starter pack file |
| `src/artwork/artwork_fetcher.*` | The background downloads: Steam's CDN for Steam, libretro-thumbnails for ROMs, iiSU's starter pack for console cards, iiSU's border pack for frame glyphs |
| `src/artwork/apk_archive.*` | iiSU's release APK read by HTTP ranges: central directory once, any entry by range with its CRC checked |
| `src/artwork/starter_pack.*` | iiSU's starter pack: its entry taken out of the APK against a pin, and a system's card as PNG |
| `src/artwork/console_glyphs.*` | iiSU's frame glyphs: `border_pack.json` read once, a system's `logo_*.png` out of the APK |
| `src/artwork/zip_archive.*` | The one zip reader: end record, central directory, local header offset, checked extraction; an in-memory archive |
| `src/net/web_client.*` | HTTPS GETs over libcurl, with headers; shared by artwork and the stores |
| `src/vdf/` | Valve KeyValues parser |
| `src/device/battery.*` | Battery level and charging state from sysfs |
| `src/gamepad/event.*` | The shell's controls (`Button`, `Event`), raylib-free (`iideck_pad`) |
| `src/gamepad/pad_translator.*` | One physical pad's evdev events as the virtual pad's and as controls |
| `src/gamepad/evdev_device.*` | An evdev node: capabilities, grab, state, reads |
| `src/gamepad/virtual_pad.*` | The uinput Xbox 360 pad a game reads |
| `src/gamepad/pads.*` | Every pad, read always and hot-plugged; holds them during a game and blocks them while the Guide menu is open |
| `src/gamepad/direction_repeat.*` | Held-direction repeat for pads and keys |

## Home UI (G002)

Pure model, unit-tested without raylib (`iideck_grid`, `iideck_hud_model`):

| Path | Owns |
| --- | --- |
| `src/ui/home_layout.*` | Grid geometry for Standard and WiiSu: cells, gaps, insets, placeholder slots, scrolling, page pill and page arrow rects (`hx2.g`, `zj2`, `ou4.q`, `ys8.h/k/l`) |
| `src/ui/grid_focus.*` | D-pad focus movement and page crossing (`hx2.z/O`) |
| `src/ui/tile_motion.*` | Focus scale, domino entrance, press pulse, ring rotation |
| `src/ui/tile_geometry.*` | One tile's rectangles and radii |
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
| `src/ui/button_glyph.*` | Controller button glyphs (`input_glyph_*`) |
| `src/ui/game_menu_painter.*` | The Guide menu over a running game |
| `src/ui/tile_painter.*` | One tile: shadow, ring, chrome, art, platform frame; a console's name tile |
| `src/ui/page_pill.*`, `page_arrow.*` | WiiSu page dots and page arrows |
| `src/ui/round_shape.*` | Tessellated rounded shapes with per-vertex colour |
| `src/ui/platform.*`, `platform_stroke.cpp` | Console border sprites, logos, stroke colours |
| `src/ui/glyph_textures.*` | The console glyphs drawn in a ROM tile's frame tab: files by system, textures on load |
| `src/ui/typeface.*`, `face_metrics.*` | Text drawing at Android em sizes; the face's line metrics |

## Tests and docs

- `tests/<area>/`: one ctest suite per owner, through the shipping code.
- `docs/re-frontier.md`: what is recovered from iiSU. `docs/reference/` (private `iideck-re`
  checkout): the evidence and the spec built on it.
