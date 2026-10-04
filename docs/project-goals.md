# Project goals

## G001 — One grid for every game you own

Present Steam, Epic, GOG and emulator ROMs in a single home screen that a
controller can drive from power-on to a running game, and launch each title into
the runtime that already owns it. iideck never bundles a game or an emulator and
never reimplements a store's authentication, download or cloud sync.

## G002 — The reference home layout

Match the iiSU single-screen layout the design is taken from: rounded glass top
bar with the focused title in a centre pill, a grid mixing square tiles with
multi-column feature tiles, page dots, and corner button hints. Tiles show real
box art and the state that matters — installed, in progress, how long since it
was played.

## G003 — Gamepad in, game out

Input is read natively from the controller rather than through the webview, so
navigation is frame-accurate and haptics are available to the shell. The shell
gets out of the way while a game runs and comes back when it exits.

## Non-goals

- A new Wayland compositor. Gamescope is the compositor.
- Replacing Steam, Legendary or Heroic as clients for their own stores.
- Shipping or scraping copyrighted game assets.