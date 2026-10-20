# More libretro cores, arcade on every romset version

Status: open. Arcade is done (below); the Sega cores and the rest of the list are next.

The player: "more libretro options, I noticed I can't play Arcade games ... since no libretro option
for it is there right now", and "new emulator versions make it so the older rom sets don't work, I
also want to make it so we can support all romset versions".

## Cores

`assets/emulators.json` lists mGBA, Gambatte, snes9x, Mesen, PCSX ReARMed and, for Arcade,
FinalBurn Neo (`fbneo_libretro`), MAME 2003-Plus (`mame2003_plus_libretro`, 0.78 sets), MAME 2010
(`mame2010_libretro`, 0.139) and MAME (`mame_libretro`, 0.289). All four are on the buildbot's
x86_64 Linux nightly folder, their core info says `hw_render = "false"`, and each loaded in
`opensu-retro` (2026-10-20); none was dropped. Current MAME's core is 416 MB, so the core zip reader
takes libraries up to 1 GB.

Next, by what the runner can already host (software rendering):

- Genesis Plus GX and PicoDrive (both on the buildbot, `hw_render = "false"`), once `rom_systems`
  has Mega Drive, Master System, Game Gear, Sega CD and 32X. Left for the next change: the
  player's ROM folder has none of those systems to prove them with.
- Beetle PCE, Beetle NGP, Beetle WonderSwan, Stella, Handy, ProSystem, melonDS DS / DeSmuME (NDS).

GPU cores (Mupen64Plus-Next, Beetle PSX HW, Flycast, PPSSPP) wait for phase 3 of
`docs/design/libretro.md` (`RETRO_ENVIRONMENT_SET_HW_RENDER`).

## Arcade romsets

Built (`library/romset_dat`, `romset_dats`, `romset_match`; `docs/design/libretro.md` §4
Romsets). A MAME romset matches one MAME version; a zip made for 0.78 fails on current MAME and the
reverse. openSU picks the core per game rather than per system:

1. Read each zip's central directory: member names, sizes and CRC32s, without decompressing.
2. Compare with each arcade core's DAT (libretro-database `metadat/mame-*`, FBNeo's own DAT): the
   core whose set for that short name has every required ROM's CRC wins; parent and BIOS zips
   (`neogeo.zip`) beside the game count as present.
3. Ties go to the player's order in Settings › Emulators (FBNeo first by default); no match lists the
   game with "No arcade core matches this romset" and the nearest version's missing files.
4. The DAT matches are cached by zip size and mtime, so a library scan reads each zip once.

The listings: FinalBurn Neo's `dats/` arcade and Neo Geo datafiles (libretro-database's copy is
v1.0.0.03, older than the nightly), `metadata/mame2003-plus.xml` and `metadata/mame2010.xml` of the
cores' libretro repositories, and current MAME's `-listxml` from the latest mamedev release
(`mame0289lx.zip`, SHA-256 checked; the core can trail a new release for a few days). Fetched over
HTTPS, kept under `<cache>/romsets/` for 30 days.

The player's arcade folder holds `armwar.zip` and `avspu.zip`. Both are 0.139-era sets: MAME 2010
matches them and runs them; FinalBurn Neo, MAME 2003-Plus and MAME want the CPS2 `.key` files (and
MAME `qsound.zip`'s `dl-1425.bin`), avspu also the later 1 MB `avp.14m`-`avp.20m` dumps. The cores'
own load errors named the same files. Proof: `scratch/arcade/` (frames and logs) and
`docs/project-state.md` S031.

The first scan, and each after a listing's 30 days, downloaded about 100 MB of listings (17.1,
22.6, 43.4 and 19.7 MB) inside the ROM scan, so the whole ROM library stayed hidden until they were
in. The player: "Make sure to reflect this as downloading somewhere". Now `library/romset_lists`
fetches them off the scan: arcade games are listed at once with "Checking romset…" (Play says the
lists are downloading), each list is a Downloads page row with its bytes and end state, each end is
toasted, and the catalog is read again when one ends. Proof: `scratch/romset-downloads/` (empty
cache; frames `02`, `10`-`15`, `armwar-runner-status.txt`, `shell-run1.log`, `shell-run2.log`).

Not done: samples are never required; 7z sets are not matched (the arcade system lists `.zip`
only). A romset list row has no Cancel or Retry button; a failed list is asked for again by the
first scan an hour later.

## Steam modes in session mode

See `integrations-opt-in.md`.
