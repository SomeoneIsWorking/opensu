# openSU's generator inside Gamescope

Asked for 2026-11-13, from the frame generation work (`remote-frame-generation.md`, "Left").

## Target

A game outside the built-in runner, played inside openSU's Gamescope with openSU's generator
chosen (`framegen.generator`), gets `flow`'s frames between its own, paced by Gamescope's
`CFramePacer` as Lossless's are. Today Gamescope's generators are Lossless and a linear blend
(`GeneratorKind`, fork `src/FrameGen/FrameGenerator.h`), so that game gets no frame generation.
Proven when such a game shows generated frames at the pacer's rate on its own screen, and when
`flow` scores at least as well as Lossless on 3D content (step c).

## Design

- **One implementation, in the fork's `framegen` library.** `flow` moves from `src/frame_gen/`
  (`flow_interpolator.cpp`, `shaders/flow_*.comp`) into the fork's MIT `framegen` library as
  `framegen::CFlowGenerator` on `framegen::CDevice` and `IQueue`, the shape of
  `CLosslessGenerator`, its shaders compiled in the library's build. openSU's `frame_gen::flow`
  wraps it as `lossless` does, for the runner, remote play and `opensu-framegen-pairs`; Gamescope
  gets `GeneratorKind::Flow` and a `CFlowSynthesizer` beside `CLosslessSynthesizer`.
- **The meter with it.** `IQueue` gains a per-dispatch timing mark (Gamescope's `CGpuTimestamps`
  and openSU's `GpuSpans` implement it), and the shaders' read counting becomes a library setting
  whose counts the host reads, so `--measure counts|time` reads the same lines.
- **Device and format.** `flow` needs robustness2's null descriptor, which Gamescope's device
  enables. Gamescope captures only 8-bit SDR commits (`IsSupportedCommit`: no YCbCr, no HDR,
  BGRA8 or RGBA8) into ABGR8888 copies, which are `flow`'s `rgba8` images; HDR stays off as it is
  for Lossless.
- **Scale and source pixel.** A Gamescope game is at its own size, mostly 3D, so `flow` runs at
  scale 1 with a 1 x 1 source pixel, moving content in whole window pixels.
- **Cost and pacing.** `flow` runs at the window's size beside the game on one GPU; the pacer's
  ready need is `flow`'s own. Measured at the GPU's idle clock before the search's merged path,
  a real frame's ingest took 3.05 ms at 1280x720 and 9.83 ms at 3840x2160.
- **Selection.** `GAMESCOPE_FRAMEGEN_GENERATOR`, read today as linear blend when not 0, becomes
  0 Lossless, 1 linear blend, 2 `flow`; `GamescopePresentation::generate` writes it from
  `FrameGenGenerator`, and `flow` needs no DLL.

Not taken: a copy of `flow` in Gamescope (a second implementation), and Gamescope loading `flow`
from openSU as a plugin (a second ABI to keep).

## Steps

- **(a) Move `flow` into the library, its behaviour the same.** A pin bump. Proven by
  `--same-as` on the core frames and every quality scene, the runner's proof run unchanged and
  the count lines the same.
- **(b) Gamescope's synthesizer and the property.** Proven by the fork's tests and a game outside
  the runner, inside openSU's Gamescope with `flow` chosen, showing generated frames and their
  pacing on its own screen.
- **(c) Quality on 3D content, first.** `flow` against Lossless and the linear blend on 3D scenes
  recorded through `opensu-retro` with 3D cores, at scale 1 with a 1 x 1 source pixel (the
  Gamescope case): a slow pan, a fast camera turn and an object crossing. If `flow` loses there
  clearly, (a) and (b) are not built, or `flow` stays the runner's.

## Step (c): measured, and (a) and (b) not built

`flow` loses to Lossless on every 3D scene, at both sizes, by more than the spread; on two of the
four it is no better than the linear blend. It stays the runner's generator, where content is
pixel art; Gamescope keeps Lossless.

Scenes, through `opensu-retro`: Ocarina of Time's title (a slow pan with a rider crossing; a fast
camera swoop over the field) and Majora's Mask's opening (the camera pulling back and turning from
the clock into the town) on the catalog's `mupen64plus_next` with ParaLLEl-RDP, and Crash
Bandicoot's title (Crash running at the camera) on the libretro buildbot's `pcsx_rearmed` (no
BIOS, its HLE one; `env-assets/cores/pcsx_rearmed_libretro.so`, sha256
`d4ace83dfe56af14f707e10d2ac6a1d93dfaca759a3f01d3c04b7c6f42d6e6d2`). None has a fast camera turn
in its attract run. Each at two sizes: "g", the Gamescope case, the N64 drawn at 4x and shown at
960x720 so every window pixel carries detail, scored with a 1 x 1 source pixel; "n", drawn at the
console's size and shown at 3x (Crash 3.75 x 3), scored with that source pixel.

The recorder writes every 60 Hz frame, but these games draw at 20 (N64) and 30 (Crash) frames a
second, so most of its frames repeat one before (the N64's with a dither of 0.27 mean luma
between them): a frame left out between two such is its neighbour, and the blend and the last
real frame scored best of all. Each scene is the game's own frames instead (a new frame where
the mean luma moves by more than 0.36, Crash's where its pixels change), every other one left
out, so the generators make the game's rate doubled; the spread is the two choices of which are
real (± half their difference). PSNR mean / worst 5% in dB, FLIP mean:

| scene | size | Lossless | `flow` | blend | repeat |
|---|---|---|---|---|---|
| oot-ride | g | 44.95 / 41.64, 0.0121 | 42.11 / 38.48, 0.0147 | 41.45 / 37.98, 0.0125 | 39.23 / 35.89, 0.0156 |
| oot-swoop | g | 38.44 / 37.00, 0.0525 | 37.41 / 36.94, 0.0603 | 37.54 / 37.20, 0.0611 | 35.61 / 34.72, 0.0716 |
| mm-town | g | 36.54 / 28.08, 0.0290 | 32.75 / 25.75, 0.0385 | 30.25 / 25.16, 0.0575 | 27.39 / 23.00, 0.0731 |
| crash-title | g | 27.19 / 18.66, 0.0455 | 25.79 / 17.60, 0.0517 | 26.38 / 18.38, 0.0547 | 24.35 / 16.14, 0.0563 |
| oot-ride | n | 45.08 / 42.85, 0.0167 | 43.34 / 39.87, 0.0178 | 43.27 / 39.69, 0.0167 | 40.62 / 37.20, 0.0192 |
| oot-swoop | n | 38.94 / 37.63, 0.0517 | 38.16 / 37.73, 0.0578 | 38.30 / 37.86, 0.0578 | 36.29 / 35.36, 0.0683 |
| mm-town | n | 34.64 / 28.52, 0.0390 | 32.28 / 26.04, 0.0480 | 32.02 / 26.12, 0.0556 | 28.86 / 23.84, 0.0705 |
| crash-title | n | 27.19 / 18.66, 0.0455 | 25.76 / 17.60, 0.0517 | 26.38 / 18.38, 0.0547 | 24.35 / 16.14, 0.0563 |

The spreads are at most 0.14 dB on the means, 0.65 on the worst 5% and 0.0014 on FLIP. `flow`
behind Lossless: 1.0 to 3.8 dB on the mean at "g", FLIP worse everywhere; only oot-swoop's worst
5% is level. The game's rate doubled is from 20 to 40 frames a second here, larger steps than a
PC game's 60 to 120; and the recorder itself scores no content drawn below the display's rate
until its frames are taken at the game's.
