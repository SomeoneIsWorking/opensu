# Forked cores: Switch, Wii U, setsail, Sunbright

Status: open, planned.

The player: "eden and ryujinx cores and cemu core, integrate setsail work as a core and integrate
sunbright work as a core too", "Ryujinx and Eden can't be on GitHub, maybe can be on gitlab",
"Convert setsail work to be a cemu fork core".

All of these draw with the GPU, so each waits for phase 3 of `docs/design/libretro.md`
(`RETRO_ENVIRONMENT_SET_HW_RENDER` with `RETRO_HW_CONTEXT_VULKAN` on the runner's own
`retro/vulkan_device`). Phase 3 comes first and is proven with an existing Vulkan core.

| Core | Source | Host | Notes |
|---|---|---|---|
| Eden | the player's fork, `gitlab.com/bobbyjunior/eden` | GitLab only | Switch; keys from the player's `prod.keys`/`title.keys`, read-only as `library/switch_keys` does |
| Ryujinx | the player's Ryubing fork, `gitlab.com/bobbyjunior/ryubing` | GitLab only | C#/.NET: the core is a native shim hosting the runtime; the largest of the set |
| Cemu | `wiiuport` (`github.com/SomeoneIsWorking/wiiuport`), Cemu fork as its `external/cemu` submodule | GitHub | the Wii U core is built from wiiuport, not a second Cemu fork |
| setsail | setsail's interpolated presentation, moved into wiiuport | GitHub | Wind Waker HD at 60 Hz becomes a per-game option of the Cemu core, not its own core |
| Sunbright | `github.com/SomeoneIsWorking/sunbright` | GitHub | Super Mario Sunshine at 60 Hz; the GameCube core with Sunshine as its pack, not a core of its own |

Each core is a libretro build target in its fork, pinned by revision in openSU the way the Gamescope
fork is, and listed in `assets/emulators.json` beside the standalone emulators. A title's
enhancement pack is switched per game by the layered settings in `retro-shaders-overlays.md`.

Order: phase 3, then Cemu (wiiuport) with setsail, then Eden, then Ryujinx, then Sunbright when it
runs. GitLab-hosted cores are built and downloaded from GitLab; nothing of them goes on GitHub.

## Later: the PSX ports

The player would like the psxport titles (`~/repo/psx`) in openSU, "but that's not feasible right
now". Not a core per port: psxport (`~/repo/psx/psxport`) is already the shared PS1 runtime
(Lightrec dynarec, devices, HLE, rendering, audio), so the core is psxport itself, a PS1 emulator
that runs any disc. Each title's native overrides and enhancements load as that game's enhancement
pack when its disc is recognised. That makes it openSU's PS1 core outright, with no fallback core,
and psxport's HLE is the PS1 HLE BIOS (`bios-provisioning.md`). Enhancement packs are the game
layer of the layered settings (`retro-shaders-overlays.md`): on by default, switchable per game.


Parked until psxport runs a game end to end as a plain emulator; until then PCSX ReARMed stays
the PS1 core.

## Emulator plus enhancements

The player: setsail is Cemu plus a Wind Waker HD 60 fps interpolator, Sunbright is Aurora plus a
Sunshine 60 fps interpolator; "they don't differ that much, I'm considering making them emulator +
enhancements". So every one of these is the same shape: one emulator core per console, and each
title's work (interpolated frames, native overrides, fixes) is that game's enhancement pack, loaded
when the disc is recognised and switched per game in the layered settings.

| Console | Emulator | Packs |
|---|---|---|
| PS1 | psxport | the psx-* titles |
| Wii U | wiiuport (Cemu) | setsail (Wind Waker HD) |
| GameCube | the player's Dolphin fork, `github.com/SomeoneIsWorking/dolphin` | Sunbright (Super Mario Sunshine) |

The interpolator itself (the game renders real in-between frames by blending camera and actor
transforms between simulation ticks) is the shared part; each pack supplies only its title's
tick, camera and actor hooks.

### The GameCube core from the player's Dolphin fork

`SomeoneIsWorking/dolphin` (fork of `dolphin-emu/dolphin`) already carries Sunbright work on two
branches (2026-10-21): `sunbright`, 33 commits ahead of `master` (GX draw and colour capture, an
indexed-XF seam, per-frame present in headless), and `sunbright-oracle`, 159 ahead (headless
FIFO record, scripted pad input, save/load state at a field, game-state JSON dumps, HTTP probes
for debugging the 60 fps interpolation). Upstream Dolphin has no libretro target; libretro's own
`dolphin` core is a separate, older fork, so the core is a libretro frontend added to this fork.

- Shipped in the core: Dolphin plus the Sunshine pack (the native operations and the interpolator).
- Kept out of the shipped core: the probes, oracle and headless capture tools; they stay
  maintainer builds for working on packs.
