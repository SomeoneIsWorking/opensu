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
| `OPENSU_LIBRETRO_THUMBNAILS` | libretro-thumbnails' root, for pointing a test run at a stand-in. |
| `OPENSU_STEAM_STAND_IN` | A root standing in for Steam's store, `/cdn`, `/items` (`GetItems`) and `/assets/`, for a test run. |
| `OPENSU_GE_PROTON_RELEASE` | The GE-Proton release to install, as GitHub release JSON, for a test run; the latest when unset. |
| `OPENSU_ROOT_STEP` | A harmless program session mode's install runs through pkexec in place of its installer, for a test run: polkit still asks openSU's agent, nothing is installed. |
| `OPENSU_POLKIT_HELPER` | A program openSU's polkit agent runs in place of `polkit-agent-helper-1`, for a test run: it gets the user as its argument and polkit's cookie on stdin and writes the helper's lines (`PAM_TEXT_INFO`, `PAM_ERROR_MSG`, `PAM_PROMPT_ECHO_OFF`/`ON`, `SUCCESS`, `FAILURE`). It cannot answer polkit, so the action itself is refused. |
| `OPENSU_SESSION_ROOT` | A folder standing in for `/` where session mode looks for its entry (`usr/local/share/wayland-sessions`, `usr/share/wayland-sessions`) and its selector (`usr/local/libexec/opensu-session-select`), for a test run: an empty folder shows Install session mode whatever this machine has installed. |
| `LEGENDARY_CONFIG_PATH` | Legendary's configuration, as Legendary itself reads it. |

The environment seeds defaults; Settings (from the Guide menu) saves your choices (interface size,
volume, shortcuts, install folders, integrations, ...) in `settings.json` and per-game settings in
`game-settings.json`, both in openSU's config folder, and those win.

## Testing polkit's questions

To bring openSU's polkit agent a BeginAuthentication on any machine, with or without session mode
installed, and change nothing:

```sh
mkdir -p scratch/polkit-test/root
DISABLE_LSFG=1 OPENSU_SESSION_ROOT="$PWD/scratch/polkit-test/root" OPENSU_ROOT_STEP=/bin/true \
    OPENSU_POLKIT_HELPER=<stand-in helper> opensu --hidden
```

Leave `OPENSU_POLKIT_HELPER` out to use the real helper and the system's `polkit-1` PAM stack.
On the control port the run logs, `POST /input` with `guide`, then `down` five times and `a`
(Power), then `down` three times; `GET /state` must show `"screenFocus": "power.installSession"`
before going on, then `a` (the Install question) and `a` (Install). pkexec then runs `/bin/true`
through polkit, which asks openSU's agent, and the card shows. A hidden run refuses Switch to
session mode and Switch to desktop whatever it is pressed ("changes are off in a hidden run"),
and the empty folder means neither is offered.

A stand-in helper takes the user as its argument and polkit's cookie as one line on stdin,
writes one line per PAM message (`PAM_PROMPT_ECHO_OFF <text>`, `PAM_PROMPT_ECHO_ON <text>`,
`PAM_ERROR_MSG <text>`, `PAM_TEXT_INFO <text>`, the text `g_strescape`d), reads each answer as one
line, and ends with `SUCCESS` or `FAILURE`; one that runs `pam_start` on its own PAM service puts
that service in front of the card. It cannot answer polkit, so polkit then refuses `/bin/true`
("This incident has been reported."), which says nothing about the card.

## Control channel

openSU listens on loopback only (port 7311, or the free port a hidden run logs). It is how the
sign-in extension hands over codes and how maintainer tools drive the shell:

| Request | Meaning |
| --- | --- |
| `GET /state` | The shell's state as JSON: section, focus, open screens, downloads, `settingsOpen`, `folderPickerOpen`, `capturingShortcut`, `volumeShown`, `chordHint`, `mapping`, `volumePercent`, `volumeMuted`, `uiScale`, `artworkPending`, `imagesPending`, `inGame`, LAN's `lanOn`, `lanSeen`, `lanPaired`, `lanCode` (the code a pairing on either side shows) and `lanSaves` (the open details page's Saves fact), `profile` (the profile played as) and `profiles` (how many) and more. |
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
