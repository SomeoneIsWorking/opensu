# Command line, environment and the control channel

## Command line

| Flag | Meaning |
| --- | --- |
| (none) | The shell. From a desktop on Linux it starts its own Gamescope, and Steam and games run inside it ([session.md](session.md)). |
| `--render FILE` | Renders one frame to `FILE` and exits; no window is shown. `--keyboard` draws the keyboard's prompts. |
| `--hidden` | A maintainer run: an unmapped window, no Gamescope, no pads and a free control port, which it logs. |
| `--steam` | With `--hidden`, starts the Steam client in its mode; end the run with `POST /quit` so Steam shuts down with it. |
| `--sound` | With `--hidden`, plays the UI sounds on the default audio device (`PIPEWIRE_NODE` can move them to another sink). |
| `--session` | openSU is the login session, started by the display manager on a top-level Gamescope. |
| `--help` | Lists these. |

Only one openSU runs per user: a second interactive start leaves the first to it.

A hidden run only reads the system volume and never starts the Steam client unless `--steam` asks
(Steam shows as stopped); it never sleeps, restarts, shuts down, scans, pairs, connects or forgets
Bluetooth devices, never writes the brightness, and starts no GE-Proton check, download or runner
test. The player's settings files are read, and its own changes kept in a folder of its own under
the runtime directory.

## Environment

All optional.

| Variable | Meaning |
| --- | --- |
| `OPENSU_STEAM_ROOTS` | Colon-separated Steam install roots. Discovered when unset. |
| `OPENSU_ROM_ROOTS` | Colon-separated ROM roots, each holding one folder per system. Discovered when unset. |
| `OPENSU_EMULATORS` | Per-system command overriding the one found: `ps2=pcsx2-qt -batch {rom};gc=dolphin-emu -b -e {rom}`. |
| `OPENSU_WIDTH`, `OPENSU_HEIGHT` | Window size, default 1280x800. |
| `OPENSU_ASSETS` | Directory holding the typeface. Defaults to `share/opensu` beside the executable's directory. |
| `OPENSU_HOME_MODE` | `standard` (scrolling grid, default) or `wiisu` (pages with peeks and page dots). |
| `OPENSU_CONTROL_PORT` | The control channel's port, default 7311. |
| `OPENSU_LAN_PORT` | The port other openSUs reach this one on while LAN is on, default 7312; a hidden run takes a free one ([lan.md](lan.md)). |
| `OPENSU_SESSION` | The name of the session's scopes, default `opensu-<pid>`; a nested session sets it for the openSU inside. |
| `OPENSU_SECRET_STORE` | `file` keeps secrets (the SteamGridDB key, LAN pairings) in an owner-only file under the config dir instead of the desktop keyring. |
| `OPENSU_STEAMGRIDDB_API` | SteamGridDB's API root, for pointing a test run at a stand-in. |
| `OPENSU_GE_PROTON_RELEASE` | The GE-Proton release to install, as GitHub release JSON, for a test run; the latest when unset. |
| `LEGENDARY_CONFIG_PATH` | Legendary's configuration, as Legendary itself reads it. |

The environment seeds defaults; Settings (from the Guide menu) saves your choices (interface size,
volume, shortcuts, install folders, integrations, ...) in `settings.json` and per-game settings in
`game-settings.json`, both in openSU's config folder, and those win.

## Control channel

openSU listens on loopback only (port 7311, or the free port a hidden run logs). It is how the
sign-in extension hands over codes and how maintainer tools drive the shell:

| Request | Meaning |
| --- | --- |
| `GET /state` | The shell's state as JSON: section, focus, open screens, downloads, `settingsOpen`, `folderPickerOpen`, `firstRunOpen`, `capturingShortcut`, `volumeShown`, `chordHint`, `mapping`, `volumePercent`, `volumeMuted`, `uiScale`, `artworkPending`, `imagesPending`, `inGame`, LAN's `lanOn`, `lanSeen`, `lanPaired`, `lanCode` (the code a pairing on either side shows) and `lanSaves` (the open details page's Saves fact) and more. |
| `POST /input` | A button name: `up down left right a b x y l1 r1 l2 r2 l3 r3 select start guide search`. `guide+a` holds buttons together; `guide 1500ms` holds them that long, up to 10000ms. |
| `POST /key` | A key combination, such as `ctrl+up`, posted to the window's event queue as a real keyboard's. |
| `POST /text` | Text typed as a real keyboard's, so Enter in a field being edited submits it. |
| `GET /frame.png` | The next frame, as PNG. |
| `POST /quit` | Closes the shell. |
| `POST /signin/gog/start`, `/signin/epic/start` | Opens the store's sign-in page ([stores.md](stores.md#signing-in-to-gog-and-epic)). |

`opensu-retro` has a channel of its own for each launch, on a free loopback port it announces in
the file `OPENSU_RETRO_CONTROL` names: `/status`, `/state/save`, `/state/load`, `/reset`, `/video`,
`/option`, `/options`, `/input`, and `/frame.png` in hidden runs. The shell's quick menu uses it.

`uv run --frozen python -m tools.screenshots` drives hidden runs through both channels to make the
README's screenshots (`--profile real` on your own library, the default `demo` on a freely
licensed one, offline).
