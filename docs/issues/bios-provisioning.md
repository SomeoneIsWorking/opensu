# BIOS files: open replacements and a download button

Status: open, research first.

The player: "for BIOS, we can use open ones, someone recently released OpenBIOS for PS2 and a
version for PSX already exists ... PS3 bios is available in Sony website and I've been told it also
carries PSX and PS2 bioses too for backwards compatibility, we can add a download button for them
maybe".

## Wanted

- Settings › Emulators lists, per system that needs one, which BIOS files the chosen core or
  emulator wants, whether each is present in `<data>/opensu/system` (or the emulator's own folder),
  and a Download button where a source exists.
- Open replacements where they exist and the core accepts them:
  - PlayStation: the PCSX-Redux OpenBIOS (MIT). Check which cores boot it (PCSX ReARMed, Beetle
    PSX, SwanStation, DuckStation) and what does not work with it.
  - PlayStation 2: the open PS2 BIOS the player mentions; find the project, its licence, its state
    and whether PCSX2 or LRPS2 boots it.
  - Other systems: list which have an open BIOS (e.g. Saturn, Dreamcast, NDS FreeBIOS in melonDS).
- Sony's PS3 system software (`PS3UPDAT.PUP`, from Sony's own site): downloaded on the player's
  request and unpacked locally to take the PlayStation BIOS it carries (and a PS2 one if it does;
  to be confirmed from the firmware's contents, not assumed). Nothing from it is redistributed.

## To find out before building

1. Which PUP packages hold `ps1_rom.bin` / PS2 images, and the decryption needed (RPCS3's
   firmware installer is the reference for the format).
2. Whether the PS1 image from the PUP boots in each PS1 core, and what region it is.
3. Each open BIOS's licence and current release, fetched as data with a checksum.

## PS2 notes

- The player is unsure how far the open PS2 BIOS gets: "it might need more work". Measure it on
  the player's PS2 games before offering it as the default; the PS3 firmware or the player's own
  dump stays the choice that works.
- Prior art in the player's repos: `ps2/avpe` built an HLE BIOS for one game (Aliens vs. Predator:
  Extinction), with an EE BIOS and IOP service inventory (`docs/issues/0020-inventory-bios-iop-service-surface.md`
  there). It covers that title's calls only, so it is reference material, not a general BIOS.
