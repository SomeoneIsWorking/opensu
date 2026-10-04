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
- The grid is ours, so layout, tile sizes and page count are ours.

## Current focus

S004 — the interactive window: a real gamepad driving focus and a game actually
launching, neither of which has run yet.

## Capability inventory

| ID | Capability or outcome | State | Factual dependency | Goals |
| --- | --- | --- | --- | --- |
| S001 | Library model and catalog aggregate over every source | verified | — | G001 |
| S002 | Steam library source: manifests, install state, artwork, playtime, favourites | verified | — | G001 |
| S003 | Gamepad input through raylib, polled per frame | partial | — | G003 |
| S004 | Launch handoff into a running game, with the shell returning | partial | S002 | G001, G003 |
| S005 | Reference home layout: top bar, tile grid, page dots, hints | verified | S001 | G002 |
| S006 | Epic source via Legendary | verified | — | G001 |
| S007 | GOG source via Heroic | verified | — | G001 |
| S008 | ROM source with per-system emulator launch | verified | — | G001 |
| S009 | Haptic rumble | partial | S003 | G003 |
| S010 | Gamescope session entry for fullscreen play | missing | — | G001 |
| S011 | Steam's own components kept out of the game grid | partial | S002 | G001 |

## Capability details

### S002 — Steam library source

Reads app manifests across every library folder, install state from both
`StateFlags` and the filesystem, artwork from `librarycache` in the modern and
legacy layouts, and playtime and favourites from every user profile.

Verified against this machine and against a synthetic install: one installed
game, one known-but-absent game, one Steam component, artwork in both layouts, a
user profile, and a second library folder listed both directly and as library
"0". It reads **37 titles with artwork for 30**.

Three real defects were found and fixed by that test rather than by inspection:
the install root was read twice because Steam lists it as library "0"; a stale
library folder won over a mounted one because content-id de-duplication ran before
the folders were checked for existence; and an app appearing under both `Apps`
and `Favorites` produced two records, so the later one won and every favourited
game's playtime was zeroed.

### S005 — Reference home layout

Renders the reference arrangement from real library data: a glass top bar with
the focused title in a centre pill, a dotted ground, a grid mixing square tiles
with multi-column feature tiles, page dots, and corner button hints. Tile
geometry is banded, so a two-row hero sits at the end of its band with squares
filling the space beside it, and the next band starts below both of its rows.
Hints appear only on the featured tiles, whose captions are shortened so the two
never overlap. A title with no artwork gets a generated card coloured from the
title, so the grid never shows a hole.

Verified by rendering frames offscreen to PNG and looking at them. That path is
how the layout became checkable at all: the shell is a Wayland window, which this
machine's screenshot tooling cannot see, and the earlier WebKit frontend's
offscreen snapshot returned an empty surface. raylib renders to a texture and
exports it, so a frame is a file.

Gap: the fonts are raylib's built-in face rather than the reference's rounded
display face, and the per-platform HSL tinting the reference applies to
artwork is not implemented.

### S003 — Gamepad input

Read through raylib, which sits on SDL and already knows the layout mainstream
pads report. Buttons are edge-detected against held state, and the left stick
also drives the dpad so the shell has one control for both.

Gap: **never exercised against a real controller.** This machine has only a
keyboard, so nothing has confirmed the button mapping. raylib 6.0 removed the
separate shoulder buttons, so the triggers carry both shoulder and axis, and the
mapping follows that.

### S004 — Launch handoff

Starts the game with its own session so a shell exit cannot reach it, hides the
window, and waits for both the spawned child and the process table, because
Steam and Legendary hand off to a different process and waiting on the child
alone would show the shell while the game is still loading. Every source records
a `processHint` — the Wine prefix path for Steam, the install folder for the
others — which is what identifies the game in the process table.

Gap: **never run against a real game.** Launching a Steam title on this machine
is the outstanding check, including whether the Wine prefix path appears early
enough to catch the process before the shell reappears.

### S009 — Rumble

`Reader::rumble` calls `SetGamepadVibration` on the first connected pad. raylib
exposes no capability query, so a pad without motors silently ignores it, and
nothing in the shell calls rumble yet.

### S010 — Gamescope session

The shell runs as an ordinary window. There is no `gamescope-session-*` entry, so
entering and leaving game mode is still the manual switch described in
`fedora-kde-steamdeck`'s installer. Nested mode works today inside the Plasma
session without any session surgery.

### S011 — Steam's own components

**Stopgap.** App manifests carry no tool-versus-game marker; that lives in
`appinfo.vdf`, whose v29 layout wraps the app section in a header block that is
not plain KeyValues and was not decoded. Components are therefore matched by the
directory names Steam installs them under, which removed 9 entries from this
machine's grid. The list drifts as Valve adds runtimes, so decoding `appinfo.vdf`
is the real fix.