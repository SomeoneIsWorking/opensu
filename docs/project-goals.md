# Project goals

## G001 — One grid for every game you own

Present Steam, Epic, GOG and emulator ROMs in a single home screen that a
controller can drive from power-on to a running game, and launch each title into
the runtime that already owns it. iideck never bundles a game or an emulator and
never reimplements a store's authentication, download or cloud sync.

## G002 — iiSU's UI and UX, replicated

Reproduce iiSU's interface and the way it behaves, not only its look, from
facts reverse-engineered out of the iiSU APK rather than guessed from
screenshots: screens, navigation, transitions, sounds, timings. The
specification is `reference/design-reference.md` (private, see `re-frontier.md`).

## G003 — Gamepad in, game out

Input is read natively from the controller rather than through the webview, so
navigation is frame-accurate and haptics are available to the shell. The shell
gets out of the way while a game runs and comes back when it exits.

iideck owns every process it starts, Gamescope included. A stuck game or a
stuck Gamescope can always be closed from the controller; no state needs a
keyboard to recover.

## G004 — Inside KDE or as its own session

iideck runs in two modes with the same behaviour: everything runs under the one
compositor that runs iideck, and closing iideck closes it all. Inside a desktop
session such as KDE Plasma, running iideck starts a nested Gamescope at the
output's own resolution with iideck, Steam and every game inside it, Alt+Tab
still returns to the desktop and Alt+F4 closes the game, not Gamescope.
As its own login session, picked at the display manager, it runs on Gamescope
or a custom compositor and owns the screen from login with no desktop under it.

As on the Steam Deck, a game can run below the output resolution while the one
Gamescope upscales it with FSR (or another of its filters), chosen per game.

## Non-goals

- Replacing Steam or Legendary as clients for their own stores. iideck signs in to GOG itself,
  lists the library and starts installs through `gogdl`; downloads stay with the store's tools.
- Shipping or scraping copyrighted game assets.
