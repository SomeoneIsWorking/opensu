# Project state

## Comparison baseline

The baseline is what this machine does today with no iideck installed: Steam's
own Big Picture (`steam -gamepadui`) launched under Gamescope, switched to by
hand from a Fedora KDE Plasma session. That UI lists Steam only. Epic needs
Legendary or Heroic in a separate window, GOG needs Heroic, and emulators and
ROMs have no home at all.

Visible deltas from the baseline:

- Epic, GOG and emulator ROMs sit in the same grid as Steam, when their runtime
  is installed.
- No store client window is ever opened to reach a game.
- The grid is our own, so layout, tile sizes and page count are ours.

## Current focus

S005 — confirming the grid renders real library entries and the controller drives
it on hardware.

## Capability inventory

| ID | Capability or outcome | State | Factual dependency | Goals |
| --- | --- | --- | --- | --- |
| S001 | Library model and catalog aggregate over every source | verified | — | G001 |
| S002 | Steam library source: manifests, install state, artwork, playtime, favourites | verified | — | G001 |
| S003 | Gamepad input read natively from evdev, hot-plug, held-state events | partial | — | G003 |
| S004 | Launch handoff into a running game, with the shell returning | partial | S002 | G001, G003 |
| S005 | Reference home layout: top bar, tile grid, page dots, hints | partial | S001 | G002 |
| S006 | Epic source via Legendary | verified | — | G001 |
| S007 | GOG source via Heroic | verified | — | G001 |
| S008 | ROM source with per-system emulator launch | verified | — | G001 |
| S009 | Haptic rumble through the evdev force-feedback interface | partial | S003 | G003 |
| S010 | Gamescope session entry for fullscreen play | missing | — | G001 |

## Capability details

### S003 — Gamepad input

Input is read from evdev rather than the webview's Gamepad API, so navigation is
edge-driven rather than polled and rumble stays reachable. Devices are discovered
by scanning `/dev/input`, filtered to those with absolute axes and all four face
buttons, and rescanned so unplugging and replugging works.

Verified: the ioctl encoding against the kernel headers, the event ABI, axis
normalisation against the range a device reports, the stick-to-dpad deadzone, and
the filter correctly rejecting this machine's mouse, keyboard and power button.

Gap: **never exercised against a real controller.** This machine has only a
Logitech K400 Plus keyboard, which the filter correctly rejects, so the button
mapping and rumble paths have no hardware confirmation. S005's own verification is
blocked on the same thing.

### S004 — Launch handoff

Launching starts the real client with the real title URL, hides the shell, and
returns when the game exits. The shell waits on both the spawned child and the
process table, because Steam and Legendary hand off to a different process and
waiting on the child alone would show the shell while the game is still loading.
Every source records a `ProcessHint` — the Wine prefix path for Steam, the
install folder for the others — which is what identifies the game in `/proc`.

Verified: process-table matching against a faked `/proc`, the refusal to start a
second game while one runs, the hide/show pair around a short-lived program, and
the refusal to launch a game with no launch command.

Gap: not yet run against a real game. A launched Steam title on this machine is
the outstanding check, including whether the Wine prefix path is present early
enough to catch the process before the shell reappears.

### S005 — Reference layout

The grid, top bar, page dots and corner hints follow the reference arrangement,
with tile sizes derived from play recency: the most recently played gets a hero
tile, the next two get wide tiles, the rest are square. Navigation is a spatial
search over tile geometry, so mixed spans move focus correctly.

Gap: **the rendered window has not been seen.** The shell builds, loads all 46
Steam entries with artwork resolved for 30 of them, and the frontend's CSS and
JavaScript are syntactically valid, but every capture attempt failed: the window
is a Wayland surface that this session's screenshot tooling cannot see, and
WebKit's offscreen snapshot returns an empty surface. The layout is therefore
unverified visually. This is the current focus.

### S006, S007, S008 — Epic, GOG and ROM sources

Epic reads Legendary's install list and skips DLC, which is not a tile of its own.
GOG reads Heroic's cached library, preferring the nested `library` title over the
outer one because that is what Heroic displays, and treats an empty library as no
games rather than a failure. ROMs scan the configured directories, map extensions
to systems case-insensitively, and append the ROM path after the emulator's own
arguments.

All three treat "client not installed or not logged in" as an ordinary state
rather than an error, so one unavailable store cannot empty the grid. Verified
against both JSON shapes Legendary and Heroic emit, DLC filtering, nested-field
precedence, install state, case-insensitive extensions, and unconfigured emulators
producing an empty launch spec.

Verified on this machine: Legendary is installed but not authenticated and
Heroic's caches are empty, so both correctly contribute nothing.

### S009 — Rumble

The evdev force-feedback path is written — the `EV_FF` capability check, an
`FF_RAMP` effect with a one-byte envelope at the documented `struct ff_effect`
offsets, and a size probe for the struct's growth across kernel versions — and
`Reader.Rumble` is bound to the frontend. The frontend does not yet call it.

Gap: no hardware to confirm an effect actually plays, and the size probe has only
been reasoned about, not exercised.

### S010 — Gamescope session

The shell runs as an ordinary windowed application. It is meant to be launched
under Gamescope, but there is no `gamescope-session-*` entry, so entering and
leaving game mode is still the manual switch described in
`fedora-kde-steamdeck`'s installer.

Gap: no session entry, and therefore no per-game frame cap or isolation. v1 nested
mode (`gamescope -e -f`) works today inside the Plasma session without any
session surgery, and is the intended launch path until this is written.

### Known defect, not yet fixed

Steam's own components appear as tiles: `Steamworks Common Redistributables`,
`Steam Linux Runtime 1.0 (scout)`, `Steam Linux Runtime 3.0 (sniper)`, `Proton
9.0` and `Proton 10.0` are 5 of the 46 entries. Steam's app manifests carry no
tool-versus-game marker; that lives in `appinfo.vdf`, whose v29 layout wraps the
app section in a header block that is not plain KeyValues and was not decoded. A
curated app id list would work but would drift as Valve adds runtimes.