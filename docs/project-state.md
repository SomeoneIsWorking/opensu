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

S004 — a game actually reaching its own window. The interactive shell now runs
and is driven, and the launch handoff has been exercised against Steam, but no
game has been observed running yet.

## Capability inventory

| ID | Capability or outcome | State | Factual dependency | Goals |
| --- | --- | --- | --- | --- |
| S001 | Library model and catalog aggregate over every source | verified | — | G001 |
| S002 | Steam library source: manifests, install state, artwork, playtime, favourites | verified | — | G001 |
| S003 | Gamepad input through raylib, polled per frame | partial | — | G003 |
| S004 | Launch handoff into a running game, with the shell returning | partial | S002 | G001, G003 |
| S005 | Reference home layout: top bar, tile grid, page dots, hints | partial | S001 | G002 |
| S012 | Loopback control channel: state, injected input, frame capture | verified | S001 | G003 |
| S006 | Epic source via Legendary | verified | — | G001 |
| S007 | GOG source via Heroic | verified | — | G001 |
| S008 | ROM source with per-system emulator launch | verified | — | G001 |
| S009 | Haptic rumble | partial | S003 | G003 |
| S010 | Own login session entry on Gamescope | missing | — | G004 |
| S013 | iideck runs in a nested Gamescope inside KDE at the output's resolution | partial | S004 | G004 |
| S014 | Alt+F4 closes the game in nested mode while Alt+Tab stays with KDE | missing | S013 | G004 |
| S015 | Shell owns every instance it starts and can force-close it from the pad | partial | S004 | G003 |
| S017 | Per-game render resolution with Gamescope FSR upscaling, Deck-style | missing | — | G004 |
| S016 | Steam client started and owned by iideck, with its state in the top bar | partial | S015 | G003, G004 |
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

Built from a screenshot before the RE pass, and it diverges from iiSU's code
(`reference/design-reference.md`): iiSU defaults to a 3×4 column-major grid that
scrolls horizontally, with no automatic feature tiles, and its own focus, frame and
motion rules. The layout below is what the shell does today, to be rebuilt from
the reference.

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

Type is Nunito Bold, which is the reference's own typeface, shipped in `assets/`
under the OFL with the licence beside it. Google Fonts publishes it only as a
variable font, and raylib loads glyphs through stb_truetype, which ignores
variation tables and would therefore render the file's default master: light
rather than bold. The shipped face is a static instance pinned to weight 700,
produced with

```sh
uvx --from fonttools fonttools varLib.instancer \
    assets/Nunito.ttf wght=700 -o assets/Nunito-Bold.ttf
```

Tile captions, badge chips and hint chips are sized from the tile's own short
side, not from the window's layout unit, so a tile's type keeps its proportion
whatever the window size or how many columns a tile spans.

Gap: the per-platform HSL tinting the reference applies to artwork is not
implemented, so covers are shown at their store's own colours.

### S003 — Gamepad input

Read through raylib, which sits on SDL and already knows the layout mainstream
pads report. Buttons are edge-detected against held state, and the left stick
also drives the dpad so the shell has one control for both.

Gap: **button mapping never exercised on real hardware.** This machine reports a
Logitech K400 Plus and its USB receiver as controllers, because SDL reports any
device with buttons as a gamepad and raylib's API cannot tell a multimedia
keyboard from a controller: it offers no way to ask how many buttons a device
has, and `IsGamepadButtonDown` cannot distinguish an unmapped button from an
unpressed one. Requiring analogue axes was tried and rejected — the K400's
touchpad is enough to give it six axes.

`IIDECK_GAMEPAD` therefore names the controller to accept, by substring, so a
player can exclude the keyboard. Verified by pointing it at a name that matches
nothing and watching the shell come up with no controller rather than with two
keyboards.

### S004 — Launch handoff

The game gets its own session so a shell exit or a hangup cannot reach it, and the
shell's window goes down before the spawn and comes back when the game leaves.
Hiding is a request to the main loop, not a call from the handoff thread: raylib's
window calls belong to the thread holding the GL context, and there is no queue
that makes them safe from elsewhere. The launch thread only raises a flag. Every source records a `processHint` — the
Wine prefix path for Steam, the install folder for the others — which is what
identifies the game in the process table.

Waiting is two phases: the game must appear, then it must leave. It is not a wait
on the child, because the child says nothing useful: a launcher that hands off
exits immediately, and one that *is* the long-lived process never exits. Steam is
both at once — `steam://rungameid/` execs `steam.sh`, which stays alive for
hours — so a child-based wait either returns instantly or never returns.

That was not a hypothetical. Driving the first launch over the control channel
left the shell reporting `launching` for 90 seconds and would have gone on for
its 12-hour timeout, with `compatdata/960090` never matching and `steam.sh` never
exiting. The appearance phase is now bounded at three minutes, and a game that
does not appear is reported as a failure.

Verified end to end through the channel: pressing play takes the window down,
Steam starts and logs in, and after exactly the three-minute appearance bound the
window comes back with `Bloons TD 6 did not start`. That is the correct outcome
on this machine, because the game genuinely does not start — the last real attempt
in Steam's log is from September and died inside Proton's prefix setup, and
`steam.sh` sits idle afterwards.

Three further defects were found by the handoff test and fixed:

- `hide` and `show` were **empty lambdas**, so the window never actually hid. The
  comment above them claimed raylib queues window calls onto the main loop; it does
  not. Every earlier run launched a game with the shell still on screen.
- A program that could not be executed was reported only after the full appearance
  bound, as "did not start", naming neither the program nor the reason. `execvp`
  returning 127 is the one signal that distinguishes "cannot run" from "not yet",
  so phase one now ends immediately on it.
- A game whose source records no hint sat out the whole bound and was then reported
  as having failed to start, while running perfectly well. `processMatches("")` is
  false by design, so there was nothing to match. A launch with no hint is now
  refused up front, because that source cannot support a handoff at all.

Gap: **still no observed game running.** The handoff's "the game appeared, now
wait for it to leave" path is therefore unproven against a real process, which is
the half that matters when it works. What is proven is that a launch that does
not happen is reported instead of hanging.

### S012 — Control channel

A loopback HTTP channel, part of the product rather than a debug flag, so an
automated run can drive the shell with no controller and no compositor in the way.
`GET /state` returns the shell's state as JSON, `POST /input` queues a button by
name, `GET /frame.png` returns the next frame as PNG bytes, `POST /quit` closes
the shell. `IIDECK_CONTROL_PORT` moves the port; it cannot be closed, because it
is how the shell is driven. It binds loopback only and names no file to read or
write — frames come back as bytes over the response.

The shell owns the GL context, so a frame request is handed to the main loop over
a condition variable and answered there rather than drawn on the request thread.
Injected buttons go through the same event path a real press takes, so what the
channel exercises is the shell's own handling and not a parallel one.

This is what made the interactive path checkable. Verified: state read, five
rightward moves walking focus across two pages, `start` returning to the first
tile, a frame captured after injection showing the focused tile's ring moved,
and `quit` shutting the shell down. The first launch through it is what exposed
the S004 wait defect.

### S009 — Rumble

`Reader::rumble` calls `SetGamepadVibration` on the first connected pad. raylib
exposes no capability query, so a pad without motors silently ignores it, and
nothing in the shell calls rumble yet.

### S010 — Own login session

There is no session entry, so iideck can only run as a window inside another
session.

### S013 — Nested Gamescope session

Outside Gamescope (`Config::insideGamescope`, from `GAMESCOPE_WAYLAND_DISPLAY`)
`session::NestedSession` runs iideck itself as `gamescope -W -H -w -h -r -f --
iideck ...` with the current monitor's size and refresh from raylib
(`session::readMonitor`), in the scope `<session>-compositor.scope` with
`IIDECK_SESSION` set for the inner iideck. When Gamescope ends it stops every
other scope named `<session>-*`, and SIGINT/SIGTERM stop the session through its
scopes. Games are never wrapped in a Gamescope of their own. The argument vector
and the session's run, leftover cleanup and signal path (against a fake
`gamescope`) are tested; no nested session has been run against a display yet.

### S014 — Alt+F4 in nested mode

KWin handles Alt+F4 and Alt+Tab as its own global shortcuts before nested
Gamescope sees them, so Alt+F4 closes Gamescope. KWin can block all global
shortcuts for a window (`gamescope --grab` or a window rule) but not one, so
that loses Alt+Tab. Planned: a KWin script, installed by iideck, that turns
Alt+F4 on iideck's Gamescope window into a game close over the control channel.

### S015 — Owned instances

Every scope iideck creates is named `<session>-<role>[-N].scope`
(`Config::session`: `IIDECK_SESSION`, else `iideck-<pid>`), and is a transient
systemd user scope (`launch::Instance`), so processes that setsid or double-fork
stay owned. A non-Steam game runs in its own scope, stopped when the game leaves
and killed when Guide is held for two seconds. A Steam game is handed to the
background client (`steam -applaunch`); Guide held for two seconds kills the
reaper whose command line carries `AppId=<id>` and everything below it
(`launch::ProcessTree`), and leaves the client up. Scope ownership, escape-proof
stop, tree kill and force-close are tested with real processes and a fake `steam`;
the Guide hold and a real Steam launch are not yet exercised.

Gap: a force-close while the client is still starting the game cannot cancel the
request it already handed to Steam.

### S016 — Steam client

`steam::Client` starts `steam -silent` in `<session>-steam.scope` when iideck
starts, if a Steam install exists, and watches it: Initializing until a logon line
(`[Logged On` with `RecvMsgClientLogOnResponse() : processing complete`) is
appended to `~/.steam/steam/logs/connection_log.txt` after the start, Failed when
the scope empties, Blocked when `~/.steam/steam.pid` names a live client outside
iideck. Steam launches wait for Ready, and are refused with a named message when
Blocked or Failed. On exit it runs `steam -shutdown`, waits up to 20 s, then stops
the scope. The state shows in the top bar (starting, ready, failed, on desktop)
and in `/state` as `steam`. Tested with a fake home and fake `steam`; a real Steam
has not been started by this code, and the ready marker is as measured on one
machine.

### S011 — Steam's own components

**Stopgap.** App manifests carry no tool-versus-game marker; that lives in
`appinfo.vdf`, whose v29 layout wraps the app section in a header block that is
not plain KeyValues and was not decoded. Components are therefore matched by the
directory names Steam installs them under, which removed 9 entries from this
machine's grid. The list drifts as Valve adds runtimes, so decoding `appinfo.vdf`
is the real fix.
### S017 — Per-game resolution and FSR

Not built. The installed Gamescope exposes the Deck's runtime controls as root
window atoms: `GAMESCOPE_XWAYLAND_MODE_CONTROL` (switch an Xwayland server's
mode, so a game sees a smaller screen), `GAMESCOPE_SCALING_FILTER` /
`GAMESCOPE_NEW_SCALING_FILTER`, `GAMESCOPE_FSR_SHARPNESS`, plus
`--xwayland-count` for a separate Xwayland per role. Their exact semantics come
from Gamescope's source (`steamcompmgr.cpp`) before any of it is used.
