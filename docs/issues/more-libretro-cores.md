# More libretro cores, arcade on every romset version

Status: open. Arcade, Nintendo DS, the Vulkan GPU cores, PS2, 3DS, GameCube and
Wii are done (below); the Sega cores and the rest of the list are next.

The player: "more libretro options, I noticed I can't play Arcade games ... since no libretro option
for it is there right now", and "new emulator versions make it so the older rom sets don't work, I
also want to make it so we can support all romset versions".

## Cores

`assets/emulators.json` lists mGBA, Gambatte, snes9x, Mesen, PCSX ReARMed and, for Arcade,
FinalBurn Neo (`fbneo_libretro`), FB Alpha 2012 (`fbalpha2012_libretro`, 0.2.97.29 sets), MAME
2003-Plus (`mame2003_plus_libretro`, 0.78 sets), MAME 2010 (`mame2010_libretro`, 0.139) and MAME
(`mame_libretro`, 0.289). All are on the buildbot's x86_64 Linux and Android arm64 nightly folders,
none asks for hardware rendering, and each loaded in `opensu-retro`; none was dropped. Current
MAME's core is 416 MB, so the core zip reader takes libraries up to 1 GB.

FB Alpha 2012 is the full core, not `fbalpha2012_cps1`, `_cps2` or `_neogeo`: its zip is 4.5 MB
(Linux) and 4.2 MB (Android) against 17 to 21 MB for FinalBurn Neo and MAME 2010, one entry and one
listing cover every driver, and no per-hardware listing exists for the split cores.

Next, by what the runner can already host (software rendering):

- Genesis Plus GX and PicoDrive (both on the buildbot, `hw_render = "false"`), once `rom_systems`
  has Mega Drive, Master System, Game Gear, Sega CD and 32X. Left for the next change: the
  player's ROM folder has none of those systems to prove them with.
- Beetle PCE, Beetle NGP, Beetle WonderSwan, Stella, Handy, ProSystem.

## GPU cores (Vulkan)

Built with phase 3 of `docs/design/libretro.md` (§4 GPU cores). Every buildbot core whose
`required_hw_api` lists Vulkan, or whose options offer a Vulkan renderer, for a system openSU has:

| Entry | Core | System | Vulkan by | Proof |
|---|---|---|---|---|
| `beetle-psx-hw-core` | `mednafen_psx_hw_libretro` | psx | `beetle_psx_hw_renderer=hardware_vk` (its "hardware" default also picks Vulkan from the preferred context) | Crash Bandicoot |
| `swanstation-core` | `swanstation_libretro` | psx | `swanstation_GPU_Renderer=Vulkan` | Crash Bandicoot 2 |
| `mupen64plus-next-core` | `mupen64plus_next_libretro` | n64 | `mupen64plus-rdp-plugin=parallel`, `mupen64plus-rsp-plugin=parallel` (default GLideN64 is OpenGL; its core info lists OpenGL only) | Ocarina of Time, Majora's Mask from the shell |
| `parallel-n64-core` | `parallel_n64_libretro` | n64 | `parallel-n64-gfxplugin=parallel`, `parallel-n64-rspplugin=parallel` | Majora's Mask |
| `ppsspp-core` | `ppsspp_libretro` | psp | `ppsspp_backend=vulkan` | none: no PSP games here |
| `flycast-core` | `flycast_libretro` | dreamcast (added to `rom_systems`: `.chd`, `.gdi`, `.cdi`) | no option; the preferred context | none: no Dreamcast games here |

The keys and values were read from the cores (2026-10-21 nightlies) and each proven one logged
`core option <key> = <value>` with no "has no value" warning. Proof in
`docs/project-state.md` S032 and `scratch/gpu-cores/proof/`. Beetle PSX HW and
SwanStation need a real PS1 BIOS (`withoutBios` needed); the runs used the OpenBIOS image copied as
`scph5501.bin`, which Beetle accepts with a SHA-1 warning. PPSSPP needs its `PPSSPP/` system files
(`ppge_atlas.zim`), not downloaded with the core. Flycast's `withoutBios` is `hle` from its core
info (every BIOS optional, `reicast_hle_bios`), not run. Unverified: PPSSPP and Flycast on a game,
and whether Flycast really picks Vulkan here.

## Nintendo DS

`assets/emulators.json` lists melonDS DS (`melondsds_libretro`, 1.4.0) as "melonDS DS (built-in)"
for the existing `nds` system (`.nds`), after the standalone melonDS. The other candidate was
DeSmuME (`desmume_libretro`). Both are in the buildbot's x86_64 Linux nightly folder (2026-10-20,
CRC-32 78c08a1d and 75dcc4c4) and both core infos say `hw_render = "true"` with every BIOS and
firmware file optional. Both ran Dawn of Sorrow in `opensu-retro --hidden` as it is: neither asked
for `RETRO_ENVIRONMENT_SET_HW_RENDER` (melonDS DS's Render Mode defaults to Software; the refused
commands, with `LUCENT_DEBUG=retro`, are rotation, rumble, username, language, controller and
subsystem info and the like). melonDS DS is taken: it boots without any file ("Falling back to
built-in firmware", "Falling back to FreeBIOS"), it reads the right stick and a pointer for the
touch screen, and it is the maintained one (DeSmuME's libretro core reports "git 95b4d79" and needs
an option to use real BIOS files). So no refusal is needed: a player's `bios7.bin`, `bios9.bin`
and `firmware.bin` would go in `<data>/opensu/system/` (not tried).

Touch screen: melonDS DS's Touch Mode defaults to Auto, joystick or pointer whichever moved last.
In joystick mode the right stick moves its cursor and R2 touches (R3 cycles its screen layouts, L2
speeds the cursor, L3 is the microphone). The runner now gives cores `RETRO_DEVICE_ANALOG`: the pads'
sticks (`gamepad::PadTranslator::sticks`, `gamepad::Pads::sticks`) through `retro/joypad`, so on a
pad the right stick and R2 work as the core intends. The runner takes no mouse, so
`RETRO_DEVICE_POINTER` reads 0 and mouse touch is not there. The layout is the core's default,
Top/Bottom, 256 × 384, letterboxed on the screen (533 × 800 at 1280 × 800); kept.

The runner's control channel has `POST /input` (`{"buttons": [...], "left": [x, y], "right":
[x, y]}`), held on a pad of its own until the next post, so a hidden run can be driven.

Proof (`scratch/nds/evidence/`, hidden shell, `OPENSU_ROM_ROOTS=/mnt/Boy/ROM`, XDG and HOME under
`scratch/nds/`): the Library's Nintendo DS tile reads 4 games and its shelf lists the four ROMs
(`shell-01-nds-console.png`, `shell-02-nds-shelf.png`); the details page offered "Download melonDS
DS (built-in)" and downloaded it into `scratch/nds/data/opensu/cores/` (log `melondsds_libretro
2026-10-20 installed … (CRC-32 78c08a1d)`, `shell-05-core-installed.png`); Play started
`opensu-retro --core …/melondsds_libretro.so --hidden --saves …/saves/nds` (`shell-06-runner-args.txt`)
whose `/status` frame went 155, 276, 397 two seconds apart (`shell-06-runner-status.txt`, again in
`run2-runner-status.txt`), and Close game from the quick menu ended it ("Castlevania - Dawn of
Sorrow closed", an 8 KB `.srm` kept; `shell-08-quickmenu-close.png`, `run2-04-closed-toast.png`).
The runner's frames of the same core show both screens (`runner-title-menu-f1300.png`; the two
cores' frame 600 and refused commands: `runner-melondsds-*`, `runner-desmume-*`). Touch: on the save menu the right stick held at (1, 1) for 20 frames then (1, 0) for 9 put the
cursor on DELETE DATA (`touch-1-cursor-on-delete-data-f1534.png`); R2 for 10 frames opened Delete
Data (`touch-2-after-r2-touch-f1600.png`, again with the final build `touch-final-after-r2.png`); the
same run without R2 stays on the menu (`touch-3-same-without-touch-f1600.png`,
`touch-final-without.png`); the runner log has `control pad holds 0 buttons, … right stick
32767,32767`.

The toast was drawn over the dock's icons (`probe.png`, `shell-04-downloading.png`,
`shell-09-after-close.png`); it now sits above the dock (`ui/hud` `drawToast` with the dock's
`DockPainter::top`; `run2-04-closed-toast.png`).

Not done: mouse touch (the runner takes no mouse), the core's options (Touch Mode, layouts) are its
defaults with no way to change them, DSi mode and real BIOS files were not tried, the microphone
reads silence.

## PS2, 3DS, GameCube and Wii

| Entry | Core | System | Vulkan by | Proof |
|---|---|---|---|---|
| `pcsx2-core` | `pcsx2_libretro` (LRPS2) | ps2 | `pcsx2_renderer=Vulkan` | Crash Twinsanity |
| `azahar-core` | `azahar_libretro` | n3ds | `citra_graphics_api=Vulkan` | Ocarina of Time 3D |
| `dolphin-core` | `dolphin_libretro` | gc, wii | no option; the preferred context | Luigi's Mansion, Donkey Kong Country Returns |

Play (`play_libretro`) is OpenGL only, so PS2 is LRPS2. Cores from the buildbot's x86_64 Linux
nightlies (2026-11-10, CRC-32 pcsx2 d6105afd, azahar a69606a5, dolphin 6b3d386d). Each run went
through `opensu-retro --hidden` on the shipping build (`scratch/builtin-cores/run/<name>/`) and
exited 0:

- PCSX2: `Bios Found: USA v01.60`, `the core draws with Vulkan`; PRESS START BUTTON, Start to the
  main menu, Cross to the memory-card prompt, Cross to "Do you really wish to format".
- Azahar: PRESS START, Start to "Please select a file.", east (3DS A) to name entry, again types.
- Dolphin: the title's PRESS START, Start to the opening, A through it to the mansion gate
  (`dolphin-gc/proof-sheet.png`). Its frames were black until the hidden window gave the core a
  surface (`docs/design/libretro.md`).
- Dolphin on Wii, Donkey Kong Country Returns: the strap notice, PRESS 2 TO PLAY; X (the remote's
  2) opened "Controls set to horizontal position", which A, Y, B and Start did not confirm
  (`dolphin-wii/proof-sheet.png`).

Gaps:

- PCSX2 keeps NVRAM, memory cards, cache and cheats under `<system>/pcsx2/`, not in the profile's
  saves folder, so they are not per profile and Copy saves cannot see them.
- Dolphin wants `<system>/dolphin-emu/Sys` (the buildbot's `assets/system/Dolphin.zip`: AR code
  handler, GC fonts, DSP ROM, game settings). Without it the core logs `Core file codehandler.bin
  missing!` and cheats are off; Luigi's Mansion ran without it. Proper fix: a catalog "system
  files" download into the system folder, as PPSSPP's `PPSSPP/` files need too.
- Azahar maps the face buttons by position (south is 3DS B).
- The core's Wii remote is its own default mapping (X is the remote's 2; A, Y, B and Start
  did nothing on the title's prompts), not the per-game table the standalone Dolphin gets (`library/wii_controls`).

## Arcade romsets

Built (`library/romset_dat`, `romset_dats`, `romset_match`; `docs/design/libretro.md` §4
Romsets). A MAME romset matches one MAME version; a zip made for 0.78 fails on current MAME and the
reverse. openSU picks the core per game rather than per system:

1. Read each zip's central directory: member names, sizes and CRC32s, without decompressing.
2. Compare with each arcade core's listing: the core whose set for that short name has every
   required ROM's CRC wins; parent and BIOS zips (`neogeo.zip`) beside the game count as present.
3. An installed matching core wins, then the catalog's order (FinalBurn Neo, FB Alpha 2012, MAME
   2003-Plus, MAME 2010, MAME); no match lists the game with "No arcade core matches this romset"
   and the nearest version's missing files.
4. The DAT matches are cached by zip size and mtime, so a library scan reads each zip once.

The listings: FinalBurn Neo's `dats/` arcade and Neo Geo datafiles (libretro-database's copy is
v1.0.0.03, older than the nightly), RetroPie's `FBA 0.2.97.29.dat` (`HerbFargus/retropie-dat`, GPL-2.0,
pinned by commit), `metadata/mame2003-plus.xml` and `metadata/mame2010.xml` of the cores' libretro
repositories, and current MAME's `-listxml` from the latest mamedev release (`mame0289lx.zip`,
SHA-256 checked; the core can trail a new release for a few days). Fetched over HTTPS, kept under
`<cache>/romsets/` for 30 days.

FB Alpha 2012's core repository holds no datafile, and libretro-database's FB Alpha dats have one
row per game with the whole zip's CRC, which a re-zipped set never matches. The 0.2.97.29 datafile
has every ROM. Against the core's driver tables (`src/burn/drv`, revision 0ce3153): 3,157 of its
3,159 sets are there and 3,132 of those compared agree; the 0.2.97.30 one has 189 sets the core
lacks. The core's own `version.h` says .30 and its core info .29.

The player's arcade folder holds armwar, avspu, mslug3, mslug5, neogeo and sfiii3n. armwar and avspu
match FB Alpha 2012 and MAME 2010 member for member. FinalBurn Neo, MAME 2003-Plus and MAME want the CPS2 `.key` files (and MAME
`qsound.zip`'s `dl-1425.bin`), avspu also the later 1 MB `avp.14m`-`avp.20m` dumps. FinalBurn Neo
without its key loads and shows its error screen, frames counting. mslug3 and mslug5 match FinalBurn
Neo, FB Alpha 2012, MAME 2010 and MAME. sfiii3n's files are in the layout only MAME 2010's listing
has. All three now also run on FinalBurn Neo through a rebuild (below). Proof:
`scratch/legacy-arcade/` and `docs/project-state.md` S031.

FinalBurn's datafiles write no `biosset`: `neogeo` lists all 29 (FB Alpha) or 38 (FinalBurn Neo) BIOS
ROMs as plain rows, so a Neo Geo game asked for every one and the player's 25-member `neogeo.zip`
read as a mismatch, though the core needs `sm1.sm1`, `sfix.sfix`, `000-lo.lo` and one system BIOS.
`RomsetDat::markChoices` now marks a BIOS's rows in a listing with no `biosset` anywhere, and the
match asks for one of them. The listing cannot tell a fixed ROM from a choice, so a `neogeo.zip`
without `sfix.sfix` still matches and the core refuses it at load. Listings with `biosset` (every
MAME) keep their exact rule, and a BIOS there without choices needs every ROM.

`neogeo.zip` was listed as a game, and Play reached FinalBurn Neo's "known but marked as not working"
screen. A zip any listing marks `isbios` is now left out of the scan; it stays in the folder the
matcher and Android's staging read. MAME 2003-Plus's listing predates the attribute, so one listing
is enough.

On a picked Android folder the scan never reached the matcher: `roms.cpp` skipped nodes that do not
open as a path, so the search's first installed core kept the game. Zips are now read through
`RomTree::open` and a parent's or BIOS's zip is staged beside the game (`--beside`). Not yet shown
on the tablet.

The first scan, and each after a listing's 30 days, downloaded about 100 MB of listings (17.1,
22.6, 43.4 and 19.7 MB) inside the ROM scan, so the whole ROM library stayed hidden until they were
in. The player: "Make sure to reflect this as downloading somewhere". Now `library/romset_lists`
fetches them off the scan: arcade games are listed at once with "Checking romset…" (Play says the
lists are downloading), each list is a Downloads page row with its bytes and end state, each end is
toasted, and the catalog is read again when one ends. Proof: `scratch/romset-downloads/` (empty
cache; frames `02`, `10`-`15`, `armwar-runner-status.txt`, `shell-run1.log`, `shell-run2.log`).

MAME 2010 crashes on load on Android, on any game by the code: the buildbot's arm64 core is built
from `jni/Android.mk`, which lacks the Makefile's `-fno-delete-null-pointer-checks`, so clang drops
the `(this)` test in `device_config::subtag` and `machine_config::detokenize` reads a null owner.
On Android openSU downloads it from openSU's own build instead: the fork
`SomeoneIsWorking/mame2010-libretro` (branch `opensu`) adds the flag, and its CI publishes each
tag's arm64-v8a and x86_64 cores as a release, named in the catalog entry's `"release"` by build.
The flag is also offered upstream (libretro/mame2010-libretro#174). Shown on an Honor phone (arm64-v8a, Android 16): MAME 2010 downloaded from release
`2026-11-07` ("openSU build 2026-11-07") and Alien vs. Predator (`avspu`) reached its attract mode;
the screen stayed black for about 35 s after Play, not yet explained. sfiii3n reaches Android through a rebuild for FinalBurn Neo
("Older romsets rebuilt for a stock core"), not yet shown on the tablet.

Not done: samples are never required; 7z sets are not matched (the arcade system lists `.zip`
only). A romset list row has no Cancel or Retry button; a failed list is asked for again by the
first scan an hour later. CHD disks are not staged on a picked folder. A vertical game's rotation
has not been run: there is no vertical set here.

## Steam modes in session mode

See `integrations-opt-in.md`.

## Older romsets rebuilt for a stock core

The player: "Make a FinalBurn core that is capable of running all ROM sets if you can". No fork:
at launch openSU rebuilds a set that matches no listing into the zip stock FinalBurn Neo asks for
(`library/romset_convert`), so the buildbot's core runs it.

Proof, 2026-11-06, a hidden shell on the buildbot's `fbneo_libretro.so` (CRC-32 92a99e81), the
player's zips (`scratch/finalburn-fork/p/`): the log has "armwar rebuilt", "avspu rebuilt", "sfiii3n
rebuilt", FBNeo's "No missing files, proceeding" and its `armwar.key` and `avspu.key` by CRC. armwar
reached its mission briefing (`armwar-play-5.png`), avspu round 1 (`avspu-play-4.png`), sfiii3n a live
round (`sfiii3n-play-4.png`). The second sfiii3n launch logged "kept". sfiii3n's panel on the runner
with `--touch` is `6 buttons (L Punch, M Punch, H Punch, L Kick, M Kick, H Kick)`. Not shown: any of
this on Android, where a picked folder is read through `RomTree` and the rebuilt zip is a local path
`GameStaging` passes through.

What a rebuild does, from `assets/romset_conversions.json`, written by `tools/romset_conversions.py`:
a zip's old name to its driver, a mirrored chip halved (`avp.14m`-`.20m` are the 1 MB dump twice),
files joined, split or padded, CPS3's 8 MB files parted into SIMMs. `assets/cps2_keys.json` supplies a
missing `.key`; 237 of FB Alpha's 240 key table rows rebuild their key file byte for byte, and a
row's name cannot be trusted (`19xx` there is `19xxu` here), so keys go by the `.key`'s CRC. Each ROM
is checked by CRC against the listing.

Coverage, of each listing's sets, by listing (`scratch/finalburn-fork/coverage-conversion.txt`):

| Listing | Sets | Stock FBNeo | With conversion |
| --- | --- | --- | --- |
| FB Alpha 0.2.97.29 | 3159 | 75.0% | 92.0% |
| MAME 0.78 | 4720 | 35.8% | 53.8% |
| MAME 2003-Plus | 6025 | 43.9% | 56.4% |
| MAME 2010 (0.139) | 8829 | 42.1% | 50.7% |

The rest stay out: FBNeo has no driver (13, 1138, 1559, 3667), or the data differs or is missing
(201, 1025, 1053, 611), such as MCUs and PROMs dumped later. So MAME 2003-Plus and 2010 stay, and FB
Alpha 2012 too: of its 253 sets out of reach, 56 run on it alone, 170 on Android with MAME 2010 hidden.

A different dump of the same chip (13 to 75 sets) is left out. FinalBurn Neo counts a ROM of another
CRC as missing, and a forged zip CRC fails at load. Its `system/fbneo/patched/` folder does take
other CRCs honestly, but a zip left there shadows the driver's real set, and no such set is here to
prove it.
