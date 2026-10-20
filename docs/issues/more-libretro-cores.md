# More libretro cores, arcade on every romset version

Status: open.

The player: "more libretro options, I noticed I can't play Arcade games ... since no libretro option
for it is there right now", and "new emulator versions make it so the older rom sets don't work, I
also want to make it so we can support all romset versions".

## Cores

Today `assets/emulators.json` lists five software cores (mGBA, Gambatte, snes9x, Mesen, PCSX
ReARMed). Next, by what the runner can already host (software rendering):

- Arcade: FinalBurn Neo, MAME 2003-Plus (0.78 sets), MAME 2010 (0.139), MAME (current).
- Genesis Plus GX and PicoDrive, once `rom_systems` has Mega Drive, Master System, Game Gear,
  Sega CD and 32X.
- Beetle PCE, Beetle NGP, Beetle WonderSwan, Stella, Handy, ProSystem, melonDS DS / DeSmuME (NDS).

GPU cores (Mupen64Plus-Next, Beetle PSX HW, Flycast, PPSSPP) wait for phase 3 of
`docs/design/libretro.md` (`RETRO_ENVIRONMENT_SET_HW_RENDER`).

## Arcade romsets

A MAME romset matches one MAME version; a zip made for 0.78 fails on current MAME and the reverse.
openSU picks the core per game rather than per system:

1. Read each zip's central directory: member names, sizes and CRC32s, without decompressing.
2. Compare with each arcade core's DAT (libretro-database `metadat/mame-*`, FBNeo's own DAT): the
   core whose set for that short name has every required ROM's CRC wins; parent and BIOS zips
   (`neogeo.zip`) beside the game count as present.
3. Ties go to the player's order in Settings › Emulators (FBNeo first by default); no match lists the
   game with "No arcade core matches this romset" and the nearest version's missing files.
4. The DAT matches are cached by zip size and mtime, so a library scan reads each zip once.

The player's arcade folder holds `armwar.zip` and `avspu.zip` to check against.

## Steam modes in session mode

See `integrations-opt-in.md`.
