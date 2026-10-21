# More libretro cores, arcade on every romset version

Status: open. Arcade, Nintendo DS and the Vulkan GPU cores are done (below); the Sega cores and the
rest of the list are next.

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
`docs/project-state.md` S032 and the worktree's `scratch/gpu-cores/proof/`. Beetle PSX HW and
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

## A FinalBurn Neo fork for older romsets

The player: "we can make a finalburn fork that can play older ROM sets". Rather than a MAME core per
romset version, a FinalBurn Neo fork whose drivers also accept the older dumps (other CRCs, names,
sizes and split files, as MAME 2003-Plus and 2010 sets carry them), so one core plays sets
across versions. The DAT matcher then lists the fork's alternates as matches. The player's armwar
and avspu (0.139 sets) are the first two to prove it; the CPS2 `.key` files they lack must still
come from the player.
