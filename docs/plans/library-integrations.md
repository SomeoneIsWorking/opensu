# Library as integrations

Status: stages 1 to 3 built (2026-11-26); 4 and 5 planned.

The player: "first we don't show any games in library view, only sub libraries like steam and
consoles and such then remove all the add buttons there and just put one add button at the end,
when selected open a screen and list integrations that can be added and I wanna remove the
integration settings menu and disabling integrations would be done by hovering an already added one
and hitting start to open a context menu with options to configure or remove or add to home.
(Adding a library itself to home, adds it as a library)". Adding one runs a wizard; for Steam:
fumarole or the Steam client, then for the client its mode with a line on each (in a login session
always openSU's own), then the login, normally or copied from another openSU on the LAN.

## What Library shows

- The sub-libraries only: each store or source in force (Steam, Epic, GOG, Your games, each paired
  openSU, Giveaways while a store shows) and each console with ROMs. No All games tile and no game
  tiles.
- One Add tile, last.
- The order stays `launcherOrder`, then the LAN hosts, then Giveaways, then the consoles.

## Add

A press on Add opens a list page, "Add to library":

- Each integration this build offers and the player has not added (`integrationEntries`), with its
  note: Steam, Epic Games, GOG, Standalone emulators, Lua plugins, LAN.
- The things that add games rather than an integration: Add a game, Add a game folder, Add a ROM
  folder, Import from a PC on your network (while LAN is on).

Picking one starts its wizard; one with nothing to ask turns on and says so.

## Wizards

Each is a short run of pages over the list, Back stepping back a page; nothing is kept until the
last page.

- Steam: "How should openSU get Steam games?" fumarole (installs and runs Steam games without the
  Steam client) or the Steam client. The client asks its mode, each with a line: Normal (your
  desktop Steam, left running), Managed (openSU's own Steam in its session, closed with it),
  Isolated (openSU's own Steam that sees only openSU's display and the controllers it is given).
  In a login session there is no choice: the session's default, Managed. The client asks for its
  own login when it starts. fumarole then signs in: a QR code the Steam mobile app scans
  (`fumarole auth --qr`, shown on its own page and refreshed as Steam changes it), this PC's Steam
  login (`--from-steam`), or later; signing in adds Steam. Copying the login from another openSU
  on the LAN is stage 5 (`/lan/profiles` carries the profile copy today).
- Epic and GOG: the sign-in (`docs/plans/store-sign-in.md`).
- Emulators, plugins, LAN: nothing to ask; turned on.
- ROM folder, game, game folder, LAN import: the flows Library's add tiles start today.

## Start on a sub-library

The tile menu (Select, the shell's menu button for a tile; Start stays the Guide menu) on a Library
tile:

- Configure: the integration's own settings page (Steam's mode and controllers, Steam libraries;
  Epic and GOG accounts; a console's ROM folders and emulator).
- Remove: turns the integration off after a question, or forgets another openSU. Only offered
  where there is something to turn off. fumarole's games are listed only once Steam is added (as
  fumarole or the client). A console has none yet: ROM folders are not
  kept per console.
- Add to Home / Remove from Home: puts the sub-library on Home as a library tile, which opens it.

## Settings

Settings › Integrations goes. Its rows move to each integration's Configure page. Until then
Configure leads to the integration's switch there, LAN for another openSU, Library's game folders
for the player's own games, and the console's settings for a console.

## Order of work

1. Library shelf: no All games, one Add tile, the Add page listing what can be added.
2. Start's context menu on Library tiles: Configure, Remove, Add to Home; library tiles on Home.
3. Wizards: Steam (fumarole or client, mode, login), the rest turning on.
4. Settings › Integrations removed, its rows on the Configure pages.
5. Steam login copied over the LAN.
