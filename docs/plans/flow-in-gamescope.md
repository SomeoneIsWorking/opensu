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
