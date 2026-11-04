# A Proton game with PROTON_ENABLE_WAYLAND shows no window

Status: open; the fix needs a Gamescope fork feature (`wp_viewporter`) and a decision.

Repro (private `kwin_wayland --virtual` on `wayland-0`, the real `CompositorSession` through
`tests/session/nested_session_driver`, GE-Proton 11-11 `notepad` started with the environment of
Gamescope's child and `PROTON_ENABLE_WAYLAND=1`): Gamescope's frame stays black; the same notepad
without the variable shows.

Cause, in two layers:

- `src/session/gamescope.cpp` `gamescopeArgs` starts Gamescope without `--expose-wayland`, so
  its children get `WAYLAND_DISPLAY=""` (Gamescope's `main.cpp` sets it to its own socket only with
  that flag; `gamescope-0` and xdg-shell exist either way). Proton's container then points Wine at
  a `wayland-0` nothing answers.
- With `--expose-wayland` Wine reaches `gamescope-0` and nothing touches the desktop's socket, but
  `winewayland` refuses the compositor: the pin lacks `wl_subcompositor` (upstream has it:
  74d13358, 255622d5, 13f1f2c2, c3b7f78c, e0ad8bed) and `wp_viewporter` (upstream lacks it too,
  ValveSoftware/gamescope#68).

What was shown: the upstream subsurface series cherry-picked onto the pin and adapted to the
fork's per-surface presentation feedbacks, plus `wlr_viewporter_create` alone, puts Notepad's
Wayland window inside Gamescope (its PipeWire frame and KWin's frame), connected only to
`gamescope-0`; X11 notepad and glxgears still show. That fork state is the local branch
`wip/wayland-clients` of the fork checkout, unpushed.

Not done, and why:

- Advertising `wp_viewporter` without applying a surface's viewport (source crop, destination
  size) when compositing is a stopgap: a Wine game whose buffer differs from its window size would
  be shown at the wrong size. The proper fix composites each surface at its viewport.
- `--expose-wayland` also moves every Wayland-capable child (SDL3, Qt, GTK emulators and native
  games) from Xwayland to Gamescope's Wayland server, where `GAMESCOPE_FOCUSABLE_WINDOWS` (Xwayland
  windows only) cannot see them, so `session::GamescopeWindows`' handoff and the overlay would lose
  them. Exposing Wayland needs either those clients pinned to X11 or the handoff reading xdg
  windows.

The openSU side is two lines once the fork is ready: `--expose-wayland` in both Gamescope command
lines, and Isolated Steam's sandbox setting `WAYLAND_DISPLAY` to Gamescope's socket.
