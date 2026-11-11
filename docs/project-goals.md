# Project goals

## G001 — One grid for every game you own

Present Steam, Epic, GOG and emulator ROMs in a single home screen that a
controller can drive from power-on to a running game, and launch each title into
the runtime that already owns it. openSU never bundles a game or a runner: no emulator,
libretro core, Wine, Proton or other compatibility layer ships inside openSU or its packages;
each is the player's own install or a download openSU fetches and verifies at run time.
openSU never reimplements a store's authentication, download or cloud sync.

## G002 — Console shell themes

openSU's look, layout and sound come as themes the player picks, each a whole console-menu feel
on one shell, driven equally well by a pad, a pointer (mouse, touch, a Wii Remote through a
DolphinBar) and a keyboard. The first two: the iiSU look already built (the default), and one
in the spirit of the Wii's menu (pages of rounded live tiles, a soft light look, rounded type,
gentle sounds, pointer-first). A theme takes a design language, not assets: art, sounds and type
are openSU's own or openly licensed, never Nintendo's or iiSU's, and no pixel-level copy. Which
theme leads, or whether openSU grows its own, is open (2026-11-11).

## G003 — Gamepad in, game out

Input is read natively from the controller rather than through the webview, so
navigation is frame-accurate and haptics are available to the shell. The shell
gets out of the way while a game runs and comes back when it exits.

openSU owns every process it starts, Gamescope included. A stuck game or a
stuck Gamescope can always be closed from the controller; no state needs a
keyboard to recover.

## G004 — Inside KDE or as its own session

openSU runs in two modes with the same behaviour: everything runs under the one
compositor that runs openSU, and closing openSU closes it all. Inside a desktop
session such as KDE Plasma, running openSU starts a nested Gamescope at the
output's own resolution with openSU, Steam and every game inside it, Alt+Tab
still returns to the desktop and Alt+F4 closes the game, not Gamescope.
As its own login session, picked at the display manager, it runs on Gamescope
or a custom compositor and owns the screen from login with no desktop under it.

As on the Steam Deck, a game can run below the output resolution while the one
Gamescope upscales it with FSR (or another of its filters), chosen per game.

## G005 — Linux, Windows, macOS and Android releases

Every release ships a Linux AppImage, a Windows installer, a macOS `.app` archive and a signed
Android APK from the same source. Outside Linux openSU is a launcher: the library, artwork, the
built-in libretro runner with its shaders, filters and setting layers, BIOS and core downloads,
standalone emulators, and Steam, Epic and GOG games started through their own clients where that
OS has them. A feature an OS cannot have is absent from its UI, not shown disabled.

Success: each package installs on a clean machine of its OS, finds the player's ROMs and store
libraries, runs a ROM on a built-in core with a shader preset, launches a standalone emulator and
a store game, and returns to the shell when it exits; saves and settings sit in that OS's
user-data location. Plan: `docs/issues/cross-platform-releases.md`.

Not part of G005: Gamescope (nested, login session, FSR, frame generation), Isolated and Managed
Steam, virtual pads remapping another program's input, Wine and session installs outside Linux;
iOS and tvOS.

## Non-goals

- Replacing Steam or Legendary as clients for their own stores. openSU signs in to GOG itself,
  lists the library and starts installs through `gogdl`; downloads stay with the store's tools.
- Shipping or scraping copyrighted game assets.
