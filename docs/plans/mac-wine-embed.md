# Wine games inside openSU's window on macOS

Asked for 2026-11-20 ("integrated launches and gamescope on macOS"); the user ruled out Screen
Recording ("I'm sure you can do it without screen recording").

## Target

A Windows game openSU starts under a Mac Wine runner (geproton-mac's GE-Proton build, plain Wine
or through `fumarole launch`) is drawn inside openSU's window, under openSU's own UI, the way a
game in openSU's Gamescope session is on Linux: the Guide opens over it, Close game ends it, the
keyboard and mouse reach it through openSU, and no window of the game shows on its own. No
permission prompt (no Screen Recording, no Accessibility).

Proven when Gravity Circuit (OpenGL) and one D3D11 game on DXMT play inside openSU's view, the
Guide shows over them, typing in the game works, and commit-to-glass inside openSU matches the
same game in its own window within run-to-run spread.

Not in scope: native Mac games. They own their windows and nothing makes them share their layers
without injecting into them, so they stay their own window with openSU raised over them
(`game_overlay_raised_window.cpp`), as today.

## How the picture gets across

Core Animation layer hosting: the game process puts its window's layers in a `CAContext`, and
openSU shows that context by its id in a `CALayerHost` in its own layer tree. The window server
composites it; no frame is copied and nothing is captured. It is the private API WebKit and
Chrome use to show their GPU process's output.

Measured on the Mac (M2, 2026-11-20) with stand-ins, a windowless Metal producer at 60 fps and a
host window (`CALayerHost`):

| | frames shown a second | commit-to-glass p50 / p95 | host CPU |
|---|---|---|---|
| producer in its own window | 60 of 60 | 11.1 / 12–20 ms | — |
| hosted in another process | 60 of 60 | 9.1–14.6 / 17–23 ms | about 0% |
| hosted, host window covered | 0 to glass | — | about 0% |

The producer kept rendering 60 a second while the host was covered, so an occluded openSU does
not stall the game; the window server just composites nothing.

## The contract

openSU and winemac.drv talk over one Unix socket per game: Lucent's `LineSocketServer`
(`lucent/line_socket.h`), the title-neutral JSON-lines channel the AGENTS rule on local control
channels points to, not `lucent::http::Server`, since winemac speaks raw lines.

- **`OPENSU_EMBED_SOCKET`**: openSU creates a listening `SOCK_STREAM` socket (in its runtime
  directory, mode 0600) before it starts the game, and sets this variable to its path in the
  game's environment; `fumarole launch` and Wine pass the environment down. Unset, winemac
  behaves as today.
- **Messages** are JSON objects, one per line, UTF-8, in both directions. Unknown fields and
  unknown kinds are ignored, so either side can add to them.
- **Units**: positions and sizes in the window's points (Cocoa's, origin top left); `scale` is
  backing pixels per point.

winemac to openSU:

- `{"hello": 1, "pid": <wine process id>}` on connect. winemac connects at a process's first
  display query (not its first window) and waits up to 2 s for `screen`, so the game's first look
  at the display is already openSU's size. Every Wine process that asks connects, so Wine's own
  `explorer.exe` says `hello` and never sends a window.
- `{"window": <id>, "context": <contextId>, "x", "y", "width", "height", "scale",
  "title": "...", "z": <order>, "kind": "normal"|"dialog"|"popup"}` when a top-level window is
  shown, moved, resized, restacked or its context changes. `x`/`y`/`width`/`height` are in Wine's
  points, relative to the screen Wine believes it has; openSU scales each window to fit its view
  and lays them out in `z` order, the highest on top. Sizes change as the game runs (a 640x400
  splash grows to 1280x720), so the scale follows each message.
- `{"window": <id>, "gone": true}` when it is hidden or destroyed.
- `{"cursor": "shown"|"hidden"}` when the game hides or shows its cursor; openSU follows it over
  the game's area.

openSU to winemac:

- `{"screen": {"width", "height", "scale"}}` once after `hello` and whenever openSU's view
  changes: the size Wine reports as the display (its desktop and fullscreen modes), in points.
  A fullscreen game then takes openSU's view's size, as a Gamescope game takes its output's.
- `{"focus": true|false}`: the game is (or is not) the foreground app, as if its window were key.
  openSU sends `false` while the Guide or another openSU screen has the input, `true` when the
  game does. A game that pauses on losing focus pauses then, as it would on Windows.
- `{"key": {"down": true|false, "keycode": <Mac virtual keycode>, "chars": "...",
  "modifiers": <NSEventModifierFlags>, "repeat": true|false}}`.
- `{"mouse": {"window": <id>, "x", "y", "buttons": <bit mask, left 1, right 2, middle 4>}}` for
  motion and button changes, in that window's points; `{"scroll": {"window": <id>, "dx", "dy",
  "precise": true|false}}`; `{"relative": {"dx", "dy"}}` while the game has captured the mouse.
- Pads are not forwarded: Wine reads them through IOKit HID whatever window is key.

## Each side

- **winemac.drv (geproton-mac).** With `OPENSU_EMBED_SOCKET` set: connect at the first top-level
  window; give each shown top-level window's content a `CAContext` and send its id; keep the
  window off the screen; report the screen openSU sends as the only display; treat `focus` as
  the window being key; turn `key`, `mouse`, `scroll` and `relative` into the same events its
  NSWindow would have delivered. Two ways to give a window a context, to measure first:
  (A) the window's own layer tree's context, the window ordered in but transparent and behind
  openSU; (B) its content view's layer moved into a new `CAContext` and the window ordered out.
  (B) is the cleaner one if OpenGL views (an `NSOpenGLContext` on a view) still draw there.
  Picked: (A).
- **openSU (darwin only).** A `GameOverlay` backend, `game_overlay_layer_host.mm`: `seesGame`
  true; the shell window's content layer gets a game layer under the UI layers holding one
  `CALayerHost` per reported window, scaled to fit the view with letterboxing; `show` keeps the
  game layer and draws the menu over it. An `EmbedSocket` owned by the launch: made before a
  Wine game starts, its path added to the game's environment, read on the shell's loop, closed
  when the game is over. Keyboard and mouse events over the game's area, while the game has the
  input, go to the socket instead of the shell. A Wine game that sends no `window` message
  within 10 s falls back to the raised window, and the log says so (`hello` alone doesn't count:
  `explorer.exe` says it for every game).
- **Shared with Linux.** The Gamescope session already composites games; it needs no socket. What
  the two share is the launch's choice to embed a game and `GameOverlay` as the owner of what
  shows over and under it; the socket and the layer hosts are macOS's.

## Steps

(a) and (b) are done in geproton-mac: (a) in b8fd0bf picked (A), the only one that shows OpenGL,
Metal and GDI windows alike; (b) in 63ba3de, proven with `tests/embed/client.py` on Vampire
Survivors (`-force-d3d11`, Player.log "Direct3D 11.0 [level 11.1]") and `tests/embed_input.c`.
Under (A) Metal's frame stats count the frames as not shown, so (A) has no commit-to-glass number
of its own. (B) with a separate CAMetalLayer measured 120 of 120 frames at p50 31.5–31.8 ms against
31.5 ms in its own window; the hosting path is the same window-server composite, so (c) proves (A)
by frames reaching openSU's view rather than by a Metal timestamp.

- **(a) winemac: one window's context, measured.** (A) and (B) on Gravity Circuit (OpenGL) and a
  D3D11 game (DXMT), each shown in a stand-in host: frames shown, commit-to-glass against the game
  in its own window. Pick one.
- **(b) winemac: the socket.** Everything above on the winemac side, behind the variable. Proven
  with a test client that hosts the windows and sends keys.
- **(c) openSU: the host view and input.** The backend, the socket, the fallback. Proven by the
  target above, with the Guide over each game and its commit-to-glass beside (a)'s.

## Results of (c) so far (Mac, 2026-11-20)

- **Vampire Survivors** (Unity 6000.0.62f1 through `fumarole launch`, from openSU's Play): hello
  from `explorer.exe` 11 s after the game ran, then `VampireSurvivors.exe` and its 1280x800 window
  2.4 s later, shown inside openSU's window at once. The player saw it live, and the mouse and the
  keyboard reached it. The Guide drew over it; Close game ended it in 5 s with nothing left.
  Without Screen Recording no picture of the composite can be taken here, so "frames reaching the
  view" is the player's eye, not a number.
- **Gravity Circuit**: on the first runner winemac put the hidden window at window level 21. The
  display's window list counts level 0 only, so the launch never counted it shown; openSU now
  counts the socket's window report as well (`Handoff::Hooks::embedded`), and winemac keeps
  embedded windows at level 0, ignoring the mouse and unable to become key (geproton 7b228e2).
  Retested on that runner: hello from `explorer.exe` 10 s after the game ran, then
  `GravityCircuit.exe` and its 1280x800 window 16 s later, counted shown 0.2 s after that and drawn
  inside openSU's window. Its window sat at level 0 with alpha 0. The player saw it live, keyboard
  and controller both drove it, and the Guide drew over it. Close game ended it in 2.5 s with no
  Wine process left.
- **Fallback, forced** with `WINEMAC_EMBED_TEST=hello-only` (hello, never a window) on Gravity
  Circuit: hellos from `explorer.exe`, `tabtip.exe` and `GravityCircuit.exe`, embed entered, and
  10 s later the warning named all three hellos and the launch's processes (`GravityCircuit.exe`,
  `fumarole-launch.exe`) and the game played in its own raised window (level 21, 2560x1440). Close
  game (through its confirmation) ended it in 2.6 s. That run's log claimed the game was shown
  inside openSU's window before any window came; the overlay now says it is waiting for one.
- **Point 4** (no Dock, Cmd-Tab or Mission Control presence): the game process is a UI element and
  `explorer.exe` a background-only process, so neither has a Dock icon or a Cmd-Tab entry. Mission
  Control and a click on the transparent window are not checked yet.
- **Not yet done**: the on-screen keyboard typing into a game with a text field, and relative mouse for a
  game that captures it (sent while the game hides its cursor; no game here tried it).
- **Guide also opens the Games app**, with or without a game: an existing macOS problem, not the
  embed's (`docs/issues/mac-guide-opens-games-app.md`).

Not taken: ScreenCaptureKit or any capture (needs Screen Recording; a covered game window may
be throttled); IOSurface frames passed over the socket (a copy per frame and a swapchain change
in DXMT, MoltenVK and wined3d each, where a layer host needs none).
