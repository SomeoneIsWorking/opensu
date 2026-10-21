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
| Cemu | `wiiuport` (`github.com/SomeoneIsWorking/wiiuport`), already a Cemu fork | GitHub | the Wii U core is built from wiiuport, not a second Cemu fork |
| setsail | setsail's interpolated presentation, moved into wiiuport | GitHub | Wind Waker HD at 60 Hz becomes a per-game option of the Cemu core, not its own core |
| Sunbright | `github.com/SomeoneIsWorking/sunbright` | GitHub | Super Mario Sunshine; waits until Sunbright itself boots gameplay |

Each core is a libretro build target in its fork, pinned by revision in openSU the way the Gamescope
fork is, and listed in `assets/emulators.json` beside the standalone emulators. A core only for one
title (Sunbright) is picked per game by the layered core choice in `retro-shaders-overlays.md`.

Order: phase 3, then Cemu (wiiuport) with setsail, then Eden, then Ryujinx, then Sunbright when it
runs. GitLab-hosted cores are built and downloaded from GitLab; nothing of them goes on GitHub.

## Later: the PSX ports

The player would like the psxport titles (`~/repo/psx`) as cores too, "but that's not feasible right
now". Parked: each port would need a libretro target in its own repo, and the ports come first as
products of their own. Revisit when a port is playable end to end.
