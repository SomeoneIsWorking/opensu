# Project goals

## G001 — One grid for every game you own

Present Steam, Epic, GOG and emulator ROMs in a single home screen that a
controller can drive from power-on to a running game, and launch each title into
the runtime that already owns it. iideck never bundles a game or an emulator and
never reimplements a store's authentication, download or cloud sync.

## G002 — iiSU's UI and UX, replicated

Reproduce iiSU's interface and the way it behaves, not only its look, from
facts reverse-engineered out of the iiSU APK rather than guessed from
screenshots: screens, navigation, transitions, sounds, timings. The home
screen has the rounded glass top bar with the focused title in a centre pill, a
grid mixing square tiles with multi-column feature tiles, page dots, and corner
button hints. Tiles show real box art and the state that matters — installed, in
progress, how long since it was played.

## G003 — Gamepad in, game out

Input is read natively from the controller rather than through the webview, so
navigation is frame-accurate and haptics are available to the shell. The shell
gets out of the way while a game runs and comes back when it exits.

iideck owns every process it starts, Gamescope included. A stuck game or a
stuck Gamescope can always be closed from the controller; no state needs a
keyboard to recover.

## G004 — Inside KDE or as its own session

iideck runs in two modes with the same behaviour. Inside a desktop session such
as KDE Plasma, games run in a nested Gamescope at the output's own resolution,
Alt+Tab still returns to the desktop and Alt+F4 closes the game, not Gamescope.
As its own login session, picked at the display manager, it runs on Gamescope
or a custom compositor and owns the screen from login with no desktop under it.

## Non-goals

- Replacing Steam, Legendary or Heroic as clients for their own stores.
- Shipping or scraping copyrighted game assets.
