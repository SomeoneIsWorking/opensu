# Ideas

Maybes the player has raised but not decided. Nothing here is planned; a decided idea moves to
`docs/plans/`.

- Run Steam games through fumarole instead of Isolated Steam, so a game is openSU's own child: no
  kept virtual pads for the sandbox, the sound sink's environment reaches it, and no client to
  hide. Windows games under Wine only until fumarole has a native `steamclient.so`; Steam Input
  and anti-cheat games would stay on the client.
- Local Steam games on Android, which needs fumarole: the Steam client doesn't run there.
- A touch layout for shooters that is all keyboard and mouse, with no pad, so a game's prompts
  never flip between the two: a swipe moves the mouse, since a right stick turns worse (Risk of
  Rain 2), and each on-screen control is a key or mouse button. Needs a virtual keyboard and mouse
  on the host, and a key for each control per game.
- Frame generation on the remote guest: the host streams at 30 fps (half the bit rate and encode
  load) or at a 60 fps game's own rate, and the guest generates the frames between up to its
  screen's rate (60, 90 or 120 Hz on phones and tablets). Needs the generator out of the Gamescope
  fork into one library both link, the guest's decoded frames in Vulkan images it can run on
  (MediaCodec's `AHardwareBuffer` imported into Vulkan, the interop SDL GPU does not offer), and
  the LSFG 3 modules of the player's own `Lossless.dll` sent from the host to their own guest.
  Interpolation holds back one real frame: about 33 ms more at 30 fps, 17 ms at 60, plus the
  generation, which a phone GPU has to fit in its frame.

## Libretro ports of standalone emulators

A libretro core of Eden (Switch) or Cemu (Wii U) would let the built-in runner own input, pacing,
shaders, presets and the Guide for those consoles, as it does for the cores it hosts. Neither exists;
each is a port of a large C++ emulator onto the libretro API (its renderer onto the runner's Vulkan
context, its threads onto `retro_run`), kept in a fork pinned by revision. Until then
`docs/plans/emulator-launch-config.md` sets the standalone builds up per launch.

## Native ports of console games

Fan-made native PC ports built from decompilations exist for some titles; openSU could offer one as
another way to run a game it lists, beside the emulator, on the player's own game data. Super Mario
Galaxy, checked 2026-11-10:

- AstroCore (Luma's Workshop): announced for 2026-10-23, never released; to be rewritten under a new
  name, no source, licence or date public. Nothing to offer or fork.
- Nebula (keepvaibin, GPL-3.0-only): 1.0.2 is Windows only (recompiles the player's RMGE01 disc).
  Nebula 2 on `main` is native from Petari with an SDL3 Linux host, unreleased; its Xbox map already
  covers the pointer (right stick), Star Bits (LB) and spin (X/Y/RB/RT), rebindable, and its
  command line (`--pak`, `--saves`, `--settings`) fits a launcher.
- Petari (the decomp, CC0-1.0): RMGE01 99.91% decompiled. Our own port would redo Nebula 2.

Next: when Nebula 2 or AstroCore's successor ships a Linux x86_64 build, offer it as a downloadable
port; record it in `docs/prior-art.md` first.
