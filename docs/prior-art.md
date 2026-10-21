# Prior art — iiSU (Android launcher, `iiSU-Alpha-7.4.apk`, release `0.0.7.4`, SHA-256 `0e9008b2b66c48f98edb7cfa42b3dbde2185ea439179ac673ed2e360769de6e8`)

iiSU is closed source; nobody else publishes a decompile. One APK patcher names classes and methods that exist in our jadx output.

| project | URL | kind | license | target build | build match (addresses checked) | what we may take | notes |
|---|---|---|---|---|---|---|---|
| iiSU | https://github.com/iisu-network/iiSU | upstream release repository (README, update notes, APK releases; no source) | none (closed source) | all releases | identity source of our APK (`gh release download 0.0.7.4`) | nothing | last push 2026-07-31 |
| Community-iiSU-PC | https://github.com/MAGOOSKEE/Community-iiSU-PC | APK patcher (apktool + smali) that redirects iiSU launches to PC emulators | PolyForm Noncommercial 1.0.0 | iiSU APK, version read at patch time | matched (2026-10-13) against our 0.0.7.4 jadx output: `MainActivity.v(MainActivity)` (`MainActivity.java:1466`), `defpackage.je6.a()` returning `MainActivity`, `defpackage.wy6.m(File, String)` returning `List` | evidence only (launch hand-off anchors) | active (2026-10-07); anchors on R8 names, which change per release |
| iiSU Interpreted | https://github.com/VictorUnlocked/iisu-interpreted-es-de | ES-DE theme imitating iiSU | none | screenshots of iiSU | not applicable | nothing | built from looks, not from the APK |
| iiSU Asset Tool | https://github.com/viik4/iisu-asset-tool | icon/cover generator for iiSU | MIT | iiSU asset conventions | not checked | evidence only (asset sizes and borders) | last push 2026-02-27 |

Not found (2026-10-13): an iiSU source release, decompile, design spec or layout documentation (GitHub, iisu.network, AlternativeTo, Patreon posts).

## BIOS and HLE (`docs/issues/bios-provisioning.md`, checked 2026-10-21)

| project | URL | kind | license | target | build match | what we may take | notes |
|---|---|---|---|---|---|---|---|
| libretro core info | https://github.com/libretro/libretro-core-info, buildbot `assets/frontend/info.zip` | each core's `.info`: `firmware*` entries, notes | MIT | every buildbot core | `pcsx_rearmed_libretro.info` lists scph5500/5501/5502 and psxonpsp660, as the core's `loadPSXBios` names them | data: the BIOS files a core looks for, read at run time | fetched with the core, kept beside it |
| PCSX ReARMed | https://github.com/libretro/pcsx_rearmed (`frontend/libretro.c`) | the PS1 core we ship | GPL-2.0 | buildbot nightly | log lines `found %s BIOS file`, `No BIOS files found.`, `HLE BIOS initialized.` seen in our runner | nothing; read for its BIOS search (named files, then any 512 KiB image with a BIOS signature) | HLE when no file is found |
| PCSX-Redux OpenBIOS | https://github.com/pcsx-redux/nugget (`openbios/`) | open replacement PS1 BIOS ROM | MIT | built `openbios.bin`: Libreboot 26.10rev3 mirror, published SHA-256 (matched), Debian `pcsx-redux-openbios` | crc32 8895083b booted 19 games in PCSX ReARMed | the binary, as an alternative only | upstream publishes no standalone build; not HLE |
| OpenPS2BIOS | https://github.com/TamaribaEmu/OpenPS2BIOS | open replacement PS2 BIOS ROM | MIT | v0.2 (2026-10-17), 4 MiB, SHA-256 published | not run (no PS2 core) | the binary, as an alternative only | PCSX2 only, reaches main menus |
| Play! | https://github.com/jpd002/Play- | PS2 emulator with an HLE BIOS, libretro core `play_libretro` | MIT (core info) | buildbot nightly | not run | the core, for a later change | no BIOS file needed |
| RPCS3 | https://github.com/rpcs3/rpcs3 (`rpcs3/Loader/PUP.cpp`, `rpcs3/Crypto/unself.cpp`, `rpcs3/Crypto/key_vault.h`, `rpcs3qt/main_window.cpp` `HandlePupInstallation`) | PS3 firmware installer | GPL-2.0 | master 4b1a801f | not run | the format as facts; keys only fetched at run time, never committed; no code copied | PUP entry 0x300, SCE package decryption |
| consolespec | https://github.com/SnowflakePowered/consolespec (`SONY_PS3/dev_flash/*.mtree`) | dev_flash file listings per firmware | none stated | PS3 4.93, 4.00, 3.74 | `ps1_rom.bin` 524 288 bytes MD5 81bbe60b… in 4.x, matching libretro System.dat | facts: sizes, hashes, no PS2 BIOS file in `ps2emu` | psdevwiki was unreachable (Cloudflare) |
| avpe | `~/repo/ps2/avpe` (the player's) | HLE BIOS for one PS2 game | the player's | Aliens vs. Predator: Extinction | not applicable | reference for PS2 HLE | covers that game's calls only |
