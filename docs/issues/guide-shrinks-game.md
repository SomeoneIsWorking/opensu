# Guide shrinks the running game

Status: open, not reproduced.

Observed (user, real session, Cuphead): after Steam's "Game already running" dialog was
closed, pressing Guide opened the menu and the game drew small at the top-left of the
screen.

Expected: the game keeps filling the screen under the menu.

Tried: Cuphead at 3840x2160 in a headless Gamescope with the user's arguments
(`-W 3840 -H 2160 -w 3840 -h 2160 -r 60 -f`), Guide opened and closed. The game window
stayed 3840x2160 and focused (`GAMESCOPE_FOCUSED_WINDOW`) throughout. Headless Gamescope
screenshots (`GAMESCOPECTRL_REQUEST_SCREENSHOT`) come back black, so the composite itself
was not seen.

What the Guide does: `session/gamescope_overlay.cpp:setShown` sets `_NET_WM_WINDOW_OPACITY`
and `STEAM_INPUT_FOCUS` on iideck's own window; it touches no game window.

Leads:
- The run that showed it had a second, failed `-applaunch` and a Steam dialog in focus
  first; the launch fix removes that sequence, so it may not recur.
- Gamescope draws an unfocused or non-fullscreen surface at its own size from the
  top-left. Check `GAMESCOPE_FOCUSABLE_WINDOWS`/`GAMESCOPE_FOCUSED_WINDOW` and the game
  window's geometry at the moment it is small, on the real display.
