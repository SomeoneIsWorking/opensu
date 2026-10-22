# An on-screen keyboard for everything: daisy wheel by default

Status: open, planned.

The player: "I want a daisy wheel keyboard (like in Xbox) as default and a regular keyboard as an
option".

Today openSU has a grid keyboard only for its own fields (`app/keyboard_walk`, `ui/search_panel`).

Decided:
- One keyboard owner with two layouts, chosen in Settings: Daisy wheel (default) and Regular (the
  existing grid, extended to a full layout).
- Daisy wheel: the left stick picks one of eight petals, each holding four characters on the face
  buttons (A/B/X/Y); triggers or bumpers switch letters, capitals, numbers and symbols; the right
  stick or d-pad moves the cursor; Start submits. Laid out per the active language.
- It types into openSU's own fields and into other apps (games, the browser). Linux, inside
  Gamescope: injected into the app in front through XTest on Gamescope's Xwayland (most games and
  the browser are X11 there); Wayland-native clients inside Gamescope to be checked. Opened with
  a Guide chord or from the quick menu, and automatically when Steam or a game asks for one if that
  request can be seen.
- Other platforms: the shell's fields only at first; Android keeps the system keyboard.
