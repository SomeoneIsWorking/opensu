# Forked cores: Switch

Status: open, planned; phase 3, which they wait for, is built.

The player: "eden and ryujinx cores", "Ryujinx and Eden can't be on GitHub, maybe can be on gitlab".
The Wii U and GameCube fork cores are dropped with setsail, wiiuport, Sunbright and the Cemu fork
(2026-11-10); those consoles stay on the downloaded Cemu and Dolphin cores and standalones.

All of these draw with the GPU. Phase 3 of `docs/design/libretro.md` is built
(`RETRO_ENVIRONMENT_SET_HW_RENDER` with `RETRO_HW_CONTEXT_VULKAN` on the runner's own
`retro/vulkan_device`, the core's extensions and features through context negotiation v2): proven
2026-10-21 with Mupen64Plus-Next, ParaLLEl N64, Beetle PSX HW and SwanStation, validation clean
(`docs/project-state.md` S032). A forked core must ask for Vulkan; an OpenGL request is refused.
What the runner does not do yet and a Switch core may need: more than one frame in flight
(`get_sync_index` is always 0), an image on another queue family (handled, not proven), a core that
submits its own command buffers through `set_command_buffers` (handled, not proven).

| Core | Source | Host | Notes |
|---|---|---|---|
| Eden | the player's fork, `gitlab.com/bobbyjunior/eden` | GitLab only | Switch; keys from the player's `prod.keys`/`title.keys`, read-only as `library/switch_keys` does |
| Ryujinx | the player's Ryubing fork, `gitlab.com/bobbyjunior/ryubing` | GitLab only | C#/.NET: the core is a native shim hosting the runtime; the largest of the set |

Each core is a libretro build target in its fork, pinned by revision in openSU the way the Gamescope
fork is, and listed in `assets/emulators.json` beside the standalone emulators. A title's
enhancement pack is switched per game by the layered settings in `retro-shaders-overlays.md`.

Order: phase 3, then Eden, then Ryujinx. GitLab-hosted cores are built and downloaded from GitLab; nothing of them goes on GitHub.

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

One emulator core per console; each title's work (interpolated frames, native overrides, fixes) is
that game's enhancement pack, loaded when the disc is recognised and switched per game in the
layered settings. The only planned one is PS1: psxport with the psx-* titles as its packs.
