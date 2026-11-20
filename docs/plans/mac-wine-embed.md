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

openSU and winemac.drv talk over one Unix socket per game.

- **`OPENSU_EMBED_SOCKET`**: openSU creates a listening `SOCK_STREAM` socket (in its runtime
  directory, mode 0600) before it starts the game, and sets this variable to its path in the
  game's environment; `fumarole launch` and Wine pass the environment down. Unset, winemac
  behaves as today.
- **Messages** are JSON objects, one per line, UTF-8, in both directions. Unknown fields and
  unknown kinds are ignored, so either side can add to them.
- **Units**: positions and sizes in the window's points (Cocoa's, origin top left); `scale` is
  backing pixels per point.

winemac to openSU:

- `{"hello": 1, "pid": <wine process id>}` on connect; one connection per Wine process that
  makes a window (the game, and a launcher it starts if that makes windows).
- `{"window": <id>, "context": <contextId>, "x", "y", "width", "height", "scale",
  "title": "...", "z": <order>, "kind": "normal"|"dialog"|"popup"}` when a top-level window is
  shown, moved, resized, restacked or its context changes. `x`/`y` are relative to the screen
  Wine believes it has; openSU lays the windows out from them, in `z` order, the highest on top.
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
- **openSU (darwin only).** A `GameOverlay` backend, `game_overlay_layer_host.mm`: `seesGame`
  true; the shell window's content layer gets a game layer under the UI layers holding one
  `CALayerHost` per reported window, scaled to fit the view with letterboxing; `show` keeps the
  game layer and draws the menu over it. An `EmbedSocket` owned by the launch: made before a
  Wine game starts, its path added to the game's environment, read on the shell's loop, closed
  when the game is over. Keyboard and mouse events over the game's area, while the game has the
  input, go to the socket instead of the shell. A Wine game whose winemac never says `hello`
  within 10 s falls back to the raised window, and the log says so.
- **Shared with Linux.** The Gamescope session already composites games; it needs no socket. What
  the two share is the launch's choice to embed a game and `GameOverlay` as the owner of what
  shows over and under it; the socket and the layer hosts are macOS's.

## Steps

- **(a) winemac: one window's context, measured.** (A) and (B) on Gravity Circuit (OpenGL) and a
  D3D11 game (DXMT), each shown in a stand-in host: frames shown, commit-to-glass against the game
  in its own window. Pick one.
- **(b) winemac: the socket.** Everything above on the winemac side, behind the variable. Proven
  with a test client that hosts the windows and sends keys.
- **(c) openSU: the host view and input.** The backend, the socket, the fallback. Proven by the
  target above, with the Guide over each game and its commit-to-glass beside (a)'s.

Not taken: ScreenCaptureKit or any capture (needs Screen Recording; a covered game window may
be throttled); IOSurface frames passed over the socket (a copy per frame and a swapchain change
in DXMT, MoltenVK and wined3d each, where a layer host needs none).
