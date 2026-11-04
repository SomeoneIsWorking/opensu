# A Proton game with PROTON_ENABLE_WAYLAND shows no window

Status: fixed.

Repro (private `kwin_wayland --virtual` on `wayland-0`, the real `CompositorSession` through
`tests/session/nested_session_driver`, GE-Proton 11-11 `notepad` started with the environment of
Gamescope's child and `PROTON_ENABLE_WAYLAND=1`): Gamescope's frame stayed black; the same notepad
without the variable showed.

Cause, in four layers:

- `src/session/gamescope.cpp` `gamescopeArgs` started Gamescope without `--expose-wayland`, so
  its children got `WAYLAND_DISPLAY=""` and Proton's container pointed Wine at a `wayland-0`
  nothing answers.
- The pinned fork lacked `wl_subcompositor` and `wp_viewporter`, which `winewayland` requires
  (upstream has the subcompositor; it has no server-side viewporter, ValveSoftware/gamescope#68).
- Gamescope's `import_commit` kept the texture it first copied from an shm buffer for as long as
  the buffer lived, so a Wine window, which draws into two shm buffers in turn, showed their first
  contents (Notepad without its edit box or status bar).
- `GAMESCOPE_FOCUSABLE_WINDOWS` listed Xwayland windows only, so once Wayland is exposed a native
  Wayland game (Qt, GTK, Wine's Wayland driver) was invisible to the handoff.

Fix:

- The fork (branch `opensu-wayland`): the upstream subsurface series adapted to the fork's
  per-surface presentation feedbacks; each surface composited at its `wp_viewport` source and
  destination (a layer's source rectangle, honoured by the composite, the DRM, Wayland and OpenVR
  planes), `wp_viewporter` hidden from Xwayland; a window's first commit no longer lost when it
  arrives before the window list is read; an shm buffer copied again at each commit; xdg
  toplevels reported in `GAMESCOPE_FOCUSABLE_WINDOWS` and `GAMESCOPE_FOCUSED_WINDOW` with their
  pid, ids with the top bit set (clear of X11's), and the window id as app id outside Steam mode,
  as X11 windows have.
- openSU: `--expose-wayland` on both Gamescope command lines, so games get `gamescope-0`; Isolated
  Steam's sandbox sets `WAYLAND_DISPLAY` to that socket. The shell stays on X11 (gpu2d asks SDL
  for it), as its overlay is an X11 property.

Shown: a 2x buffer with a cropped source and a stretched subsurface (the fork's
`tests/viewport_client`) composited at its viewport; Proton's Wayland notepad, X11 notepad and
glxgears in the nested session; KWrite (Qt, native Wayland) launched from a hidden openSU, reported
with its pid, focused, the Guide opened and closed over it and Close game returning to Home;
nothing but Gamescope connected to the desktop's `wayland-0`.

Left: a hidden openSU never maps its window, so the shell drawn as the overlay over a Wayland game
was not seen in a frame (an X11 `STEAM_OVERLAY` stand-in over a Wayland window was). SDL3 programs
(`opensu-retro`) and Dolphin stay on Xwayland by their own choice.
