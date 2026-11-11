# A ROM folder with loose files shows nothing

Status: fixed on the desktop; the tablet re-check is the player's.

Observed on the tablet with 0.6.0: picking a folder that holds `link_clock.gba` directly adds nothing
to Home, and no message was on screen 5 s later. Moved into `gba/` inside that folder, the
game shows. Reproduced on the PC in a hidden run (2026-11-11, `scratch/rom-loose/`): a folder of
four loose cartridge ROMs and two loose discs, picked through Settings › Library › Add a ROM folder,
toasted "ROM folder added", the catalog loaded 0 games and Home held only the add tiles
(`b07-picked.png`, `b09-home.png`).

Cause: `roms.cpp:Provider::list` read only the root's subfolders `systemForFolder` names and skipped
every file at the top of the root, and `settings_controller.cpp:addRoot` said "ROM folder added"
without looking at what the folder held.

## Fix

- Each system in `rom_systems` lists the extensions only its games use (`RomSystem::loose`, a subset
  of its `extensions`): the cartridge formats, `.gcm`, `.wbfs`, the Wii U and Switch files, `.gdi`
  and `.zar`. `readRoot` reads a root's top once: the folders named for a system, the loose files
  `systemForFile` places by those extensions, and a count of the files with a game extension that
  places nothing (`.iso`, `.chd`, `.cue`, `.rvz`, `.cso`, `.zip`, ...). `Provider::list` lists both
  kinds through the same per-entry code; the left-out count is logged per root.
- Discs are not placed by header: `dump_info` only recognises a Wii disc (the magic at 0x18), not
  GameCube, PS1, PS2 or PSP, so a loose disc is left out and counted. `.rvz` is GameCube's and the
  Wii's, so it is left out too.
- Adding a folder through the chooser (`RomFolderSettings::add`) says what it holds: "No games found
  in <folder>: put each console's games in a folder named for it, like gba/", or with discs left
  out "N files need a folder named for their console, like ps2/" (after "ROM folder added;" when
  other games were found). `holdsGames` stops at the first system folder with a game.
- A toast wider than the screen now wraps (`Hud::drawToast`); before, the empty-folder message ran
  off both edges.

Seen in a hidden run on the build with the fix: the loose folder toasted "ROM folder added; 2 files
need a folder named for their console, like ps2/", 4 games loaded and Home showed NES, Super
Nintendo, Game Boy and Game Boy Advance with a game each (`c03-loose-picked.png`, `c05-home.png`);
an empty folder (a `readme.txt` only) toasted the no-games message on three lines
(`c01-empty-picked.png`), and "ROM folder removed" kept its one-line toast (`c08-removed.png`).
Focused test: `roms` (`looseFilesArePlaced`).
