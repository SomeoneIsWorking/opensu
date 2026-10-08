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
| S003 | Gamepad input from evdev, hot-plugged, with iiSU's repeat | partial | — | G003 |
| S004 | Launch handoff into a running game, with the shell returning | partial | S002 | G001, G003 |
| S005 | iiSU home grid: Standard (Flow) and WiiSu (Paged) modes, top bar, prompts | partial | S001 | G002 |
| S012 | Loopback control channel: state, injected input, frame capture | verified | S001 | G003 |
| S006 | Epic source via Legendary | verified | — | G001 |
| S007 | GOG source via Heroic | verified | — | G001 |
| S008 | ROM source with per-system emulator launch | verified | — | G001 |
| S009 | Haptic rumble | missing | S003 | G003 |
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

### S005 — Home grid

Rebuilt from iiSU's decompiled code (`reference/iisu/home-grid.md`, `motion.md`,
`input-sound.md`), not from screenshots. `IIDECK_HOME_MODE` picks the mode:

- `standard` (default) is iiSU's Flow grid: 3 rows, column-major, square cells,
  scrolling horizontally with iiSU's lead margin and eased scroll (`hx2.g`,
  `hx2.L`, `hx2.P`).
- `wiisu` is the Paged grid: whole 3-row pages sized by `zj2`, the next page
  peeking past a 36 dp gap, page changes instant, and the page pill (`wf7`).

Owners: `HomeLayout` (geometry, pure), `GridFocus` (neighbour search with
remembered row and column, then iiSU's pixel pass over every page's frames, which
is how focus crosses pages; pure), `tile_motion` (focus scale, domino entrance,
pulse, ring rotation, scroll easing; all time-based, pure), `TilePainter` (shadow,
sweep focus ring, glass chrome, cover-cropped art, platform frame, fallback
letter, in `tx2`/`tw2` draw order, always the dark variant as iiSU's home config
fixes `darkHeroScrim`) and `PagePillPainter` (dark variant). The shell composes
them under `Hud`: iiSU's single-screen top bar (`TopBarMetrics` for is7/hs7/a32.o
sizes, `StatusPillPainter` for the bell, clock, battery and R2 glyph, `ClockText`
for o28.g's format and k42's minute tick), the corner hints and iideck's toast.
The battery comes from `device::BatteryReader` (sysfs, system scope only); the
12/24-hour choice from the LC_TIME locale in `config`. Home shows no title pill,
as iiSU's does not. There are no feature tiles or badges; iiSU has none on the
home grid.

Verified by `tests/ui` (layout numbers hand-computed from `hx2.g` and `zj2`,
neighbour and page-crossing rules, motion curves, tile geometry from `tj2.V`,
top-bar sizes at 640/853/1280 dp, clock formats and battery icon slots),
`tests/device` (battery parsing from a fake sysfs) and by rendering both modes
offscreen at 1280×800 and 1920×1080 and looking at them.

Stopgaps, each marked in code:

- dp is `min(W/853, H/480)`; iiSU's density comes from Android.
- WiiSu's 12 dp horizontal padding is not applied and `zj2` takes the grid gap as
  its spacing; the call site's arguments are unresolved.
- `hx2.z`'s cross-flow fallback for multi-lane tiles is not ported; every home
  tile is 1x1.
- Top bar: glass is the fill only (no blur, border or shadow); the `jj2.w` strip
  is not drawn; the bell is drawn at the progress ring's size; the status text row
  is centred after the bell; text ink is the icons' `#4D4655`.
- Corner prompt panels: the right panel shows "A Select" without iiSU's "+ Menu",
  because iideck has no START menu; their glass is the fill only, like the top bar's.
- The battery is re-read on the clock's minute tick; there is no uevent listener.

Gaps: items are ordered by install state and recency rather than iiSU's user
arrangement; WiiSu placeholders do not take focus as they do in iiSU; held
keyboard directions repeat every frame. The top bar's R2 glyph and bell hint at
notifications iideck does not have, and "B Back" is shown as iiSU shows it although B
does nothing on Home.

Offscreen rendering is how the layout became checkable at all: the shell is a
Wayland window, which this machine's screenshot tooling cannot see. raylib
renders to a texture and exports it, so a frame is a file.

Type is Cal Sans 1.000, iiSU's default face (`pp4.a`; the others it offers are Console Sans
and Arctainium), shipped in `assets/` under the OFL with the licence beside it. The file is the
Google Fonts build, which matches the one in iiSU's APK in metrics and Latin-1 outlines. Text
sizes are Android's: the em in pixels, with letter spacing in ems; `Typeface` converts to
stb_truetype's ascent-to-descent pixel height from the face's `hhea` table.

Gap: the per-platform HSL tinting the reference applies to artwork is not
implemented, so covers are shown at their store's own colours.

### S003 — Gamepad input

`gamepad::Pads` reads every evdev node that is a gamepad (`BTN_SOUTH` and a left
stick), so a keyboard is never one, and watches `/dev/input` with inotify so a pad
switched on later is picked up; it skips iideck's own and Steam Input's virtual pads.
`gamepad::PadTranslator` maps the kernel's gamepad codes to the shell's controls,
the left stick and hat included, so any driver following them works (xpad, xone,
hid-playstation, hid-nintendo). Held directions, from a pad or the keyboard, move
once and then repeat after 400 ms every 100 ms (`gamepad::DirectionRepeat`, iiSU's
95 ms throttle on Android's repeats). Tested: `pads` against uinput pads (hotplug,
Steam's virtual pad ignored, disconnect), `direction_repeat`, `pad_translator`.

The raylib/GLFW reader this replaced had no mapping for the xone driver's Xbox
controller, so its buttons never registered, and counted a Logitech K400 Plus as a
controller; the keyboard path moved focus on every frame a key was down.

Gap: not yet confirmed on the real Xbox controller.

### S004 — Launch handoff

The game gets its own session so a shell exit or a hangup cannot reach it, and the
shell's window goes down once the game appears in the process table and comes back
when it leaves; a launch whose game never appears leaves the shell up.
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

Missing: the raylib reader that had it is gone; evdev force feedback is the way back.

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
stay owned. A non-Steam game runs in its own scope,
stopped when the game leaves. A Steam game is handed to the background client
(`steam -applaunch`). Close game in the Guide menu force-closes either: the scope,
or for Steam the reaper whose command line carries `AppId=<id>` and everything below
it (`launch::ProcessTree`), leaving the client up. Scope ownership, escape-proof
stop, tree kill and force-close are tested with real processes and a fake `steam`;
a real Steam launch is not yet exercised.

Guide while a game runs opens a menu down the left edge over the dimmed game:
Resume, Close game (`ui::GameMenu`, `GameMenuPainter`); Up/Down move, A selects,
B or Guide resumes, and other buttons do nothing while a game runs. Inside Gamescope
iideck's window stays mapped as Gamescope's overlay (`session::GamescopeOverlay`:
`STEAM_OVERLAY`, `_NET_WM_WINDOW_OPACITY` 0 while the menu is closed, and
`STEAM_INPUT_FOCUS` while it is open); its window is sized to the output and has an
ARGB visual, which is why it has no MSAA. Outside Gamescope the window is hidden
during a game and shown for the menu. Verified headless: `gamescope --backend
headless` running iideck with a ROM whose emulator execs `glxgears`, driven over the
control channel (launch, guide, down, a), with full-composition screenshots
(`GAMESCOPECTRL_REQUEST_SCREENSHOT` = 3 on the root; `gamescopectl screenshot`
drops overlay planes). Driver: `scratch/overlay-test/drive.py`.

From launch until the launch ends, `gamepad::Pads` holds every gamepad, including one
connected meanwhile: each
physical pad is grabbed (`EVIOCGRAB`) and the game reads one uinput Xbox 360 pad
(045e:028e) per physical pad instead, translated by `gamepad::PadTranslator` (axes
rescaled to xpad's ranges, a button d-pad as the hat, digital triggers as full
axes). Guide never reaches the game; the shell reads its controls from the same
pads, held or not. A virtual pad is ready only once udev has opened it to the user,
so a game enumerating at once can open it. While
the menu is open the virtual pads rest (held keys released, axes centred); on
resume they take up the held axes, and keys held then stay up until pressed again.
A game iideck starts gets `SDL_GAMECONTROLLER_IGNORE_DEVICES_EXCEPT=0x045e/0x028e`,
so SDL hides the grabbed pads. Tested: `pad_translator` (unit), `pads`
against real uinput pads, `handoff` passing the environment; verified headless with
a uinput pad driving the shell (`scratch/overlay-test/drive_pad.py`): B reaches the
game while playing, Guide and all menu input do not, the virtual pad is gone after.

Gaps: a Steam game gets Steam's environment, not the SDL hint; Steam Input reads
hidraw where a pad has it, which a grab does not cover (an xone pad has none, and
Steam then reads the virtual pad); a non-SDL game that enumerates every evdev pad
sees the grabbed, silent one too.

Gap: a force-close while the client is still starting the game cannot cancel the
request it already handed to Steam.

### S016 — Steam client

A Steam game with a pending update is downloaded by Steam before it runs: its
appmanifest carries StateFlags bit 2 (UpdateRequired) and
`BytesToDownload`/`BytesDownloaded`/`BytesToStage`/`BytesStaged`. While that holds,
the launch keeps the shell up, toasts `Updating <title> · N%`, and its three-minute
appearance bound does not run; B cancels. Reproduced from a real run: BTD6
(960090) with a 2.2 GB update left an empty Gamescope behind a hidden shell.


`steam::Client` starts `steam -silent` in `<session>-steam.scope` when iideck
starts, if a Steam install exists, and watches it: Initializing until a logon line
(`[Logged On` with `RecvMsgClientLogOnResponse() : processing complete`) is
appended to `$HOME/.steam/steam/logs/connection_log.txt` after the start, Failed when
the scope empties, Blocked when `$HOME/.steam/steam.pid` names a live client outside
iideck. Steam launches wait for Ready, and are refused with a named message when
Blocked or Failed. On exit it runs `steam -shutdown`, waits up to 20 s, then stops
the scope. The state shows at the top bar's left, in the friends slot iideck leaves
empty (starting, ready, failed, on desktop)
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
