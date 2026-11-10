# Standalone emulators set up for each launch

Requested 2026-11-10: "we should have this [automatic Cemu pad config] for other non-libretro
emulators like Switch ones or also maybe for all of them", and "Eden ... launches asking for update,
which can't be dismissed without a mouse or keyboard ... we should still be able to update
emulators. We can make our own updater".

## What it does

- Every standalone emulator openSU starts takes input from the virtual pads the game is given, with
  no setup in the emulator: the player's own profile bound to those pads when one exists, a default
  pad profile written when none does.
- No emulator stops on a dialog a pad cannot close at start: its own update check, first-run
  wizard or "new version" prompt is off for openSU's launches.
- The player's own files come back byte-identical when the emulator ends; a crash's leftovers are put
  back at the next launch. Starting the emulator outside openSU behaves as before.
- Updates come from openSU: Settings › Emulators checks and installs each emulator's official
  build (`library/emulator_release`, `app/emulator_downloads`), for every emulator that publishes
  one.

## Order

1. One owner for launch-time config: what `library/cemu_pads` does for Cemu (set for the run, put
   back after, leftovers recovered), extended to a per-emulator rule, with Cemu its first user.
2. Eden and its family (Citron, Sudachi): the Eden update prompt reproduced from boot and turned off for the
   launch; `qt-config.ini`'s `player_N_*` SDL bindings retargeted to the virtual pads' GUID and
   port, and a default profile when the player has none.
3. Each other standalone emulator, each with a reproduction of whether a virtual pad reaches the game today
   and what its config binds by (SDL index, name or GUID): Ryujinx, Dolphin, PCSX2, RPCS3,
   DuckStation, PPSSPP, Azahar, melonDS, xemu, Xenia Canary, shadPS4, mGBA. One emulator per change.
4. Release sources for every emulator with an official Linux build, so openSU's own Update covers
   them, and an update openSU finds shown on Downloads.

## Libretro versions

Where a libretro core of the same emulator exists (Dolphin, LRPS2, PPSSPP, melonDS DS, SwanStation,
Citra), offering it as a built-in core sidesteps step 3 for that console: the runner owns its input.
That is `docs/issues/more-libretro-cores.md`'s list. No core exists for Switch, Wii U, PS3, PS4 or
Xbox 360; a libretro port of Eden or Cemu is its own project (`docs/ideas.md`), not part of this plan.
