# ROMs and standalone emulators

## ROM folders

ROMs live in a ROM root with one folder per system (`PS2`, `GameCube`, `Switch`, `PSX CHD`, ...).
openSU finds the roots itself: a folder named `ROM`, `ROMs` or `roms` in the home folder, in
`~/Emulation`, or at the top of a mounted drive. Settings › Library › Add a ROM folder adds one
(on Android through the system's folder picker), and Find ROM folders automatically forgets the
added ones. `OPENSU_ROM_ROOTS` overrides the search ([environment.md](environment.md)).

A system folder holds game files or game folders; in a folder, the base game is started rather than
its updates and DLC. Library has one tile per console, and each console's shelf its games.

![A console's shelf](../screenshots/console-shelf.webp)

A PS4 dump and a Switch game are titled and shown by their own files: a Switch NSP or XCI is read
with the keys Eden uses (`~/.local/share/eden/keys/prod.keys` and `title.keys`), offline; the keys
are only read, never copied, logged or sent.

## The emulator for each game

Built-in cores ([libretro.md](libretro.md)) are always available. Standalone emulators are an
integration, off until turned on in Settings › Integrations. Then they are found on PATH, as
AppImages in `~/Applications`, `~/AppImages` or `~/.local/bin`, or as Flatpaks: Dolphin, Cemu,
Eden, Ryujinx, Citron, sudachi, PCSX2, RPCS3, shadPS4, DuckStation, PPSSPP, melonDS, Azahar, mGBA, xemu and Xenia
Canary. On Windows and macOS openSU finds and launches the installed ones (macOS in
`/Applications`); on Android it starts an installed emulator app by intent with the game.

A game whose system has no emulator still appears, and pressing play names the emulator to
install. The details page's Emulator row picks one per game, Settings › Emulators per console (both
are setting layers, so a console's pick shows on its games as inherited), and Settings › Library ›
Reset emulator choices forgets every game's own pick. `OPENSU_EMULATORS` overrides the command per
system.

## Copying saves between emulators

When two or more installed emulators keep a save of a Switch game, its details page shows Copy
saves, naming them. It opens a card: Copy from and Copy to each show an emulator's saves with their
newest change and size (A lists them), and Copy saves asks, then replaces the target's saves of
that game with the source's: the current profile's save and the device save, where both keep one.
The target's saves are backed up first under `~/.local/state/opensu/save-backups/switch/<title
id>/<emulator>/<time>/`, and Undo copy on the same card puts the newest backup back exactly as it
was. Eden, Citron and sudachi are read from `~/.local/share/<name>/nand`, Ryujinx from
`~/.config/Ryujinx/bis`; a game is matched by the title id in its own file, so it needs the Switch
keys above. An emulator needs a save of the game to be copied to: start the game in it once.
Close the emulators before copying.

## Emulator downloads

Emulators that publish an official AppImage (Cemu, Eden, PCSX2, RPCS3, DuckStation, PPSSPP, Azahar,
mGBA, xemu) can be downloaded from a ROM's details page ("Download <emulator>") or from Settings ›
Emulators, into `~/.local/share/opensu/emulators/<id>/`. A download is checked against the
release's published SHA-256 and refused on a mismatch; without one, only HTTPS from the project's
own host is accepted and the checksum seen is recorded. Settings › Emulators also checks for
updates (nothing updates by itself) and removes openSU's AppImages. openSU links no BIOS, keys or
firmware. An AppImage needs FUSE (`/dev/fuse`, `fusermount`); one built on the older runtime also
needs libfuse2, and openSU names the package to install when it is missing.

Every download, with its progress, is on the Downloads page ([stores.md](stores.md#downloads)).
