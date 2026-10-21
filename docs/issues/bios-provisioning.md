# BIOS: HLE first, the core's files shown, a real BIOS as the fallback

Status: open. Built: Settings › Emulators shows each chosen built-in core's BIOS mode and files.
Not built: the PS3 system software extraction. It waits for a game that fails under HLE, and it
would be a separate tool, not openSU code.

The player: "for BIOS, we can use open ones ... PS3 bios is available in Sony website ... we can
add a download button for them maybe", then "an open bios is not the same as an HLE bios and I
think I'd prefer HLE if we can do it", and BIOS/HLE work "drifts from openSU category into
emulation category, probably needs its own repos".

## Order of preference

1. HLE: the core emulates the BIOS's calls itself; no file at all.
2. A real BIOS the player has, or one taken from Sony's PS3 system software on the player's press
   (not built yet).
3. An open replacement BIOS (a ROM image of its own, as PCSX-Redux OpenBIOS or melonDS's FreeBIOS):
   recorded as an alternative only. It is not HLE.

openSU's part is choosing cores, showing each core's BIOS mode and files, and the optional
real-BIOS download. No emulator or HLE code goes in openSU, and no core patches.

## Built: the BIOS rows in Settings › Emulators

`library/core_bios`, `library/libretro_cores`, `app/emulator_settings`, catalog `"withoutBios"`.

- A core's BIOS files are the `firmware*` entries of its libretro core info. A core download fetches
  the buildbot's `assets/frontend/info.zip` (same origin, hops checked) and keeps `<core>.info`
  beside `<core>.so`. A core downloaded before this has its info fetched by Check for updates. A
  core installed another way has its info looked for in `~/.config/retroarch/info` and
  `/usr/share/libretro/info`.
- What a core does without a file is a catalog fact, `"withoutBios"`: `hle` (mGBA, PCSX ReARMed),
  `open` (melonDS DS: FreeBIOS and generated firmware are an open replacement, not HLE), `skipped`
  (Gambatte, Snes9x, Mesen: boot ROMs or add-on BIOSes the games don't need), or `needed` by
  default. Arcade cores show no rows, because their BIOS sets are romsets.
- For each built-in core that is a system's chosen emulator, the page shows:
  - a "<Systems> BIOS" row whose value is the mode: "HLE built in", "Open BIOS built in",
    "Not needed", "Missing", or "Your BIOS file <name>" when a listed file is in
    `<data>/opensu/system`;
  - one row per listed file with its description, marked Present or Missing.
- Proof, in a hidden shell on a scratch data dir (`scratch/bios/proof/`):
  - `02-bios-before.png`: PlayStation BIOS reads "HLE built in" and its four files read Missing,
    right after PCSX ReARMed and its info were downloaded through Settings.
  - `03-bios-after.png`: with a file at `system/scph5501.bin`, the row reads "Your BIOS file
    scph5501.bin" and that file reads Present. The file was the OpenBIOS image (below), copied
    there under that name for the proof only.
  - Tekken 3 started from the shell:
    - with the file, the core log reads `found US BIOS file ... scph5501.bin` and
      `Loaded BIOS` (`05-runner-bios.log`); the runner's `/status` reached frame 2445;
    - without it, the log reads `No BIOS files found.` and `HLE BIOS initialized.`
      (`06-runner-hle.log`); `/status` reached frame 1826.
- A core that takes BIOS images from the folder by its own rule says so in the catalog
  (`"biosScan": "pcsx_rearmed"`). `library/core_bios` (`pcsxRearmedBiosIn`) applies the same rule
  as PCSX ReARMed's `frontend/libretro.c` at c8816799 (`loadPSXBios`, `try_use_bios`,
  `find_any_bios`):
  - It tries the core's own names first: scph5500, scph5501, scph5502, psxonpsp660, scph101,
    scph7001 and scph1001, each `.bin`. If that did not find all three regions, it tries every
    file in the folder, in readdir order.
  - It skips a name containing "unirom" and a file that is not exactly 512 KiB.
  - It needs `00 08 3c 3f` at offset 1 or "PS compatible" at 0x12c.
  - The region comes from 0x7ff51: " E" is EU; " J", or one of three JP CRC-32s, is JP; anything
    else is US. The first file of each region is kept.
  - The rows then name every file the core will take ("In use · US"), a listed file it rejects
    reads "Not used", and an unlisted one it takes gets its own row ("Found in the folder").
  - Proof: `proof/07-bios-scan-openbios.png` shows `openbios.bin`, under its own name, as "Your BIOS
    file openbios.bin" and "In use · US". Tekken 3 started from the shell then logged
    `found US BIOS file, crc32 8895083b: …/openbios.bin` and `Loaded BIOS "…/openbios.bin"`
    (`proof/08-runner-openbios.log`); `/status` reached frame 1824.

## PS1: HLE measured on the player's games

PCSX ReARMed (buildbot 2026-10-21), HLE (`No BIOS files found.`, `HLE BIOS initialized.`). Each of
the 19 games in `/mnt/Boy/ROM/PSX CHD/` was run hidden in `opensu-retro` for 3600 frames, with
Cross and Start pressed in turn from frame 900. Frames are in `scratch/bios/hle/`, and the
contact sheet is `scratch/bios/hle-sheet.jpg`.

- All 19 started and exited 0, with no crash or hang.
- Reached a title, menu, intro or demo by frame 3600:
  - C-12 (title);
  - Crash 1, 2 and 3 (title menus);
  - Crash Bash (game type menu);
  - CTR (main menu);
  - Digimon World (intro FMV);
  - Mega Man X4 (intro);
  - Spider-Man and Spider-Man 2 (title);
  - Spyro 2 (demo mode);
  - Spyro the Dragon and Spyro 3 (in a level and in demo mode);
  - Starfighter Sanvein (intro);
  - Tekken 3 (intro FMV);
  - Tomba! and Tomba! 2 (intro);
  - Vagrant Story (title menu).
- Disney-Pixar Toy Story 2 was still on the Disney logo at frame 3600. At 7200 frames it reached
  its "Press Start" title (`scratch/bios/hle-long/`), so it is slow to start, not failing.
- Not measured: gameplay past the menus, memory-card saves and loads under HLE, and the BIOS shell
  (memory card manager, CD player), which HLE has none of.
- No game failed under HLE, so nothing is on the reference list for psxport's HLE yet, and there is
  no case where OpenBIOS fixes an HLE failure.

OpenBIOS on PCSX ReARMed, the same 19 games and presses, with `openbios.bin` in the system folder
(picked up by the core's folder scan: `found US BIOS file, crc32 8895083b`):
- All 19 exited 0. Frames are in `scratch/bios/openbios/` and the sheet is
  `scratch/bios/openbios-sheet.jpg`.
- Spyro 2 showed a black frame at 3600, where HLE was in its demo mode. Not investigated.
- The rest reached title, menu, intro or demo frames like HLE's.

## PS3 system software: findings, extraction not built

Decision (2026-10-21): not built. All 19 of the player's PS1 games run under HLE, so there is no
measured need. It waits for a game that HLE fails, and when built it is a separate tool that writes
the BIOS into `<data>/opensu/system`, not openSU code. openSU's rows then show the file like any
other.

- Sony's download is `PS3UPDAT.PUP` 4.93, 206 197 916 bytes, listed in
  `http://fus01.ps3.update.playstation.net/update/ps3/list/us/ps3-updatelist.txt` and linked from
  playstation.com.
  - It is HTTP only: the CDN hosts answer TLS with an Akamai certificate for another name.
  - Sony publishes no digest. The ETag's hex probably is the MD5, but that is unverified.
- `ps1_rom.bin` is at `dev_flash/ps1emu/ps1_rom.bin`. In firmware 4.00 to 4.93 it is 524 288 bytes,
  MD5 `81bbe60ba7a3d1cea1d48c14cbcc647b` (consolespec's dev_flash mtree; libretro System.dat crc
  `2F53B852`). Firmware 3.74 and earlier has a 4 089 584-byte version.
  - PCSX ReARMed would take the 4.x file through its folder scan if it passes the core's signature
    check. That is unconfirmed without the file.
  - DuckStation accepts it but has no hash entry for it, so it shows as an unknown BIOS.
  - Its region is not stated anywhere I could read; Beetle calls it region-free.
- There is no PS2 BIOS file in the PUP. `dev_flash/ps2emu` in 4.93 holds only `ps2_emu.self`,
  `ps2_gxemu.self` and `ps2_netemu.self`.
  - Third-party scripts carve a PS2 BIOS out of a decrypted SELF, or pad the pre-4.00 4 MB
    `ps1_rom.bin`. Neither is official or verified here.
- Format, from RPCS3's installer (GPL-2.0; read for the format, not copied):
  - The PUP is an `SCEUF` header with file and HMAC-SHA1 tables. Entry 0x300 is
    `update_files.tar`, a plain ustar.
  - Each `dev_flash_*.tar.aa.*` member is an SCE package. Its metadata info is decrypted with
    AES-256-CBC using the SCEPKG key and IV, then its metadata with AES-128-CTR. Its sections are
    decrypted with AES-128-CTR, and a section is zlib-inflated when it is marked compressed.
    Section 2 is an inner tar holding `dev_flash/...`.
  - The `ps1emu` package's number in 4.93 is not known; it has to be found by scanning the
    packages.
- Keys: `SCEPKG_ERK`, `SCEPKG_RIV` and the PUP HMAC key are public in RPCS3's
  `rpcs3/Crypto/key_vault.h`. Keys may not be committed here, so openSU would fetch that header at
  a pinned revision, check its SHA-256 and read the keys from it at run time.
- Estimated size, about 500 lines of C++ plus tests:
  - PUP reader and HMAC check;
  - ustar reader;
  - SCE header and metadata decryption over libcrypto (already linked for Switch);
  - section decryption and inflate;
  - the pinned key fetch;
  - an HTTP-only download job with a size check, a Downloads row and deletion of the PUP.
- The PUP was not downloaded: with no extractor there was nothing to prove with it.
- A 4.x `ps1_rom.bin` is 512 KiB; PCSX ReARMed would take it by its folder scan if it passes the
  signature check, and the rows would name it.

## PS2 (no core yet; recorded for the next change)

- HLE: Play! (MIT, libretro core `play_libretro`) runs PS2 games with its own HLE BIOS; no file.
- Real BIOS: PCSX2 and LRPS2 need one. LRPS2 scans `system/pcsx2/bios` for 4 to 8 MiB images that
  pass PCSX2's ROMDIR/ROMVER check.
- Open replacement: OpenPS2BIOS (`github.com/TamaribaEmu/OpenPS2BIOS`, MIT, v0.2 2026-10-17, 4 MiB
  with a published SHA-256).
  - It is tested only in PCSX2: 38 games listed, all "reaches main menu", none playing.
  - It has no system menu, memory card browser, PS1 mode or DVD video. Unimplemented syscalls
    halt the game.
  - An alternative, not the default.
- Prior art: the player's `ps2/avpe` HLE BIOS covers one game's EE/IOP calls (Aliens vs. Predator:
  Extinction). It is reference material, not a general BIOS.
- From the PS3 firmware: the player points at a comment in the r/emulation thread "Open source PS2
  BIOS" (`old.reddit.com/r/emulation/comments/1x8iv0c/`, not readable from here) saying the PS2
  BIOS is in the PS3 firmware. That fits the PUP findings above: no BIOS file, but the `ps2emu`
  programs carry one inside, which unofficial scripts cut out. To measure when a PS2 core exists:
  whether such an image passes PCSX2's ROMDIR/ROMVER check and boots the player's games.

## Other systems

| System | Chosen core | Without a file |
|---|---|---|
| GBA, GB, GBC | mGBA | HLE BIOS (GBA); GB boot ROMs skipped |
| GB, GBC | Gambatte | boot ROMs skipped |
| SNES | Snes9x | none needed (BS-X and Sufami Turbo BIOSes are for add-ons only) |
| NES | Mesen | none needed (`disksys.rom` is for Famicom Disk System only) |
| NDS | melonDS DS | open replacement: FreeBIOS and generated firmware, not HLE |
| Arcade | FinalBurn Neo, MAME | BIOS sets are romsets (`neogeo.zip`), matched per game |

## Emulation work, separate repos (notes only)

- PS1: PCSX ReARMed's built-in HLE is the near-term path. The long-term PS1 HLE BIOS is psxport's
  own (`~/repo/psx/psxport`), because psxport is planned to become openSU's PS1 core with per-game
  enhancement packs (`forked-console-cores.md`). Games that fail under PCSX ReARMed's HLE become
  the reference list for psxport's HLE; none failed in the measurement above. The PS3 real BIOS is
  the fallback. No PCSX ReARMed fork for HLE.
- PS2: start from Play!'s HLE core. Fork it only when one of the player's games shows a defect in
  it.
- HLE lives in the cores, not in a standalone HLE BIOS: an HLE BIOS has to trap BIOS calls inside
  the emulator, so a separate one would need every core patched anyway.
