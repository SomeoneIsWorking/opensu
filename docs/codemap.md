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
| `src/app/control_channel.*` | Loopback HTTP control channel (`lucent::http::Server`) |
| `src/config/config.*` | The one reader of the environment, into typed immutable config |

## Session and processes (G003, G004)

| Path | Owns |
| --- | --- |
| `src/session/nested_session.*` | Re-running iideck inside its own Gamescope when not already in one |
| `src/session/gamescope.*` | The Gamescope command line |
| `src/session/monitor.*` | The output's size and refresh |
| `src/session/gamescope_overlay.*` | iideck's window as Gamescope's overlay over a running game |
| `src/launch/instance.*` | One transient systemd user scope per launch; stopping it ends the whole tree |
| `src/launch/handoff.*` | Starting a game in an instance and hiding the shell until it ends |
| `src/launch/process_tree.*` | Finding processes by command line, ending trees |
| `src/launch/command.*`, `argv.hpp` | Short-lived children, executable lookup, exec argv |
| `src/launch/steam_gate.hpp` | What a Steam launch waits for from the owned Steam client |
| `src/steam/client.*` | The Steam client iideck owns: start in background, readiness, state |
| `src/steam/desktop_steam.*` | Detecting a Steam client running outside iideck |

## Library

| Path | Owns |
| --- | --- |
| `src/library/game.*` | The launchable `Game` record |
| `src/library/catalog.*` | Building the catalog from all sources |
| `src/library/steam.*`, `epic.*`, `gog.*`, `roms.*` | One source each |
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
| `src/ui/button_glyph.*` | Controller button glyphs (`input_glyph_*`) |
| `src/ui/game_menu_painter.*` | The Guide menu over a running game |
| `src/ui/tile_painter.*` | One tile: shadow, ring, chrome, art, platform frame |
| `src/ui/page_pill.*`, `page_arrow.*` | WiiSu page dots and page arrows |
| `src/ui/round_shape.*` | Tessellated rounded shapes with per-vertex colour |
| `src/ui/platform.*`, `platform_stroke.cpp` | Console border sprites, logos, stroke colours |
| `src/ui/typeface.*`, `face_metrics.*` | Text drawing at Android em sizes; the face's line metrics |

## Tests and docs

- `tests/<area>/`: one ctest suite per owner, through the shipping code.
- `docs/re-frontier.md`: what is recovered from iiSU. `docs/reference/` (private `iideck-re`
  checkout): the evidence and the spec built on it.
