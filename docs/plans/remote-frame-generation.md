# Frame generation on the remote play guest

Asked for 2026-11-11: the host streams at a lower rate (30 fps: half the bit rate and half the
encode load) or at the game's own 60, and the guest generates the frames in between, up to its own
screen's rate (60, 90 or 120 Hz on phones and tablets).

## Target

- A remote game on a guest shows motion-interpolated frames between the host's real ones, made by
  the same LSFG 3 generator the Gamescope fork runs (`docs/issues/adaptive-lsfg-vk.md`), paced to
  the guest's display: a 30 fps stream shows about 60 frames a second on a 60 Hz guest, every
  generated one visibly between its two neighbours.
- The host can stream at 30 fps instead of 60 when the guest asks for it.
- Off by default, per game and global, through the frame generation settings the local game
  already has.
- First on a Linux guest with PyroWave; Android and Windows follow (see "Order").

## Where the generator lives

One library, `framegen/` at the top of the Gamescope fork (`SomeoneIsWorking/gamescope`, MIT
inside the BSD-2 tree, as `src/FrameGen/` is now), built on its own by CMake or meson with no
Gamescope header: the pacer (`CFramePacer`), the Lossless generator (shader loading and interface
checks, `PeResources`, `SpirvInterface`, pipelines, images, the synthesizer) and a small device
seam (`framegen::VulkanContext`: instance, physical device, device, `vkGetDeviceProcAddr`, the
compute queue's command buffers, submit with a sequence number, wait for one). Images go in and
out as raw Vulkan images and views in `GENERAL` layout.

- Gamescope links it in-tree; what stays in `src/FrameGen/` is Gamescope's: capturing the focused
  window's commits (`CFrameGenerator`), the linear-blend stand-in on Gamescope's compositor, and an
  adapter from `CVulkanDevice` and `CVulkanTexture` to the seam.
- openSU builds it from the fork at the commit `cmake/ThirdParty.cmake` already pins
  (`OPENSU_GAMESCOPE_COMMIT`), into the shared third-party prefix, and finds it as
  `OpensuFrameGen`. The guest and the host's Gamescope therefore always run one revision of one
  generator; there is no second copy and no second pin.
- Why not a repository of its own: it would add a pin that can drift from the fork's, and the
  fork is the one third-party repository this work may push to. The cost is that a guest build
  fetches the fork's tree (its own commit, no submodules) to build one directory; if that cost
  matters for Android or Windows, the directory splits out with its history (`git subtree split`)
  and both consumers move to the new pin in one change.
- Why not Lucent or gpu2d: Lucent holds title-neutral helpers, gpu2d the SDL GPU 2D layer; this is
  a raw Vulkan compute generator with its own licence note and its own consumers.

## How the shaders reach the guest

The LSFG 3 modules are the player's: read from their own `Lossless.dll` on their own host, sent
only to their own paired guest, never written to the guest's disk, never packaged, committed or
uploaded.

- The host's LAN listener answers `/lan/framegen/modules`, a signed call with a sealed answer
  (`docs/design/lan.md`) open to paired peers only: the DLL's FileVersion and the SPIR-V resources
  303 to 400 (both size modes, FP16 and FP32), read in place with the library's `PeResources`.
  Without a DLL on the host it answers why, and the guest's frame generation stays off with that
  reason.
- The guest fetches them once per remote session when frame generation is on, keeps them in
  memory, and hands them to the library through the same loader that checks every module's
  interface against `docs/design/frame-generation-spec.md`; a mismatch refuses with the same line
  the fork logs.
- The guest's own `Lossless.dll`, when it has one (a Linux guest with Lossless Scaling in Steam),
  is not used: the host's copy is the one the player paired.

## The guest's pipeline (Linux)

Stage A (first):

1. PyroWave decodes as now (its own device, read back into RGBA).
2. `RemoteFrameGen` (new, `src/app/remote_frame_gen.*`) owns a Vulkan device of its own on the
   same GPU, the library's synthesizer and pacer, three real-frame images and the generated
   images. Each real frame's RGBA is uploaded and added (`AddRealFrame`); the pacer's plan
   generates the in-between images; each output is read back to RGBA.
3. `RemotePicture` shows whatever the pacer picks for the present about to happen, through its
   existing RGBA upload.

Stage B (after A is proven): no CPU copies. PyroWave decodes on the generator's device
(`pyrowave_create_device`, `pyrowave_decoder_decode_gpu_buffer` into plane images), a compute pass
turns BT.709 YCbCr into RGBA, and the outputs are exported as DMA-BUFs that the shell's SDL GPU
device samples through gpu2d's DMA-BUF import (the SDL fork's), an RGBA single-plane variant of
`gpu2d::DmabufPicture`.

## Pacing and latency

- The library's `CFramePacer` paces, unchanged. A real frame's ready time is its host capture time
  on the guest's clock (`GuestStream::timing`'s offset), so network jitter does not read as a rate
  change; its arrival time is when its decode finished, which places it on the output timeline.
- The vblank grid is the shell window's presents: each loop pass asks the pacer for the output of
  the present it is about to make. A hidden run's 60 Hz tick stands in for the vblank.
- `ShellWindow` presents at most 60 a second today; while a remote game generates, the cap follows
  the display's refresh so a 90 or 120 Hz screen gets its rate.
- Added latency, by the pacer's design: one real interval held back (33.3 ms at 30 fps, 16.7 ms at
  60), the largest recent arrival lag, the generator's ready need and up to a refresh, and the
  generation itself. The ready need (`frame_gen::ReadyNeed`) is how long after a real frame
  arrives the generator has its first frame between ready, measured on the timeline semaphore
  (`ReadyWatch`) over the last 64 real frames: their slowest and 1 ms, raised at once by a slower
  frame, lowered by at most 2 ms after a whole window under it, a refresh until the window is
  full. The pacer plans the first generated frame at least that long after its real frame
  (`SetReadyNeededNs`); with a need of 0, as Gamescope's own generator runs, the first is due as
  little as 3.4 ms after it at 50 fps to 60 by the arrival's phase (`pacer_first_output`). The
  generation itself (2.2 ms per generated 720p frame balanced on an RX 6700 XT, more on a phone GPU, which
  must fit it inside its refresh). Stage A adds an upload per real frame and a readback per shown frame.
- Below 10 fps, while measuring, or within the tolerance of the target, real frames show as they
  come, as in the fork.
- Planned: a decoded frame waits for the shell loop's next pass before it reaches the generator,
  4 to 13 ms after its decode (`h-pace`, `h-grab2`); the decode thread waking the generator
  removes that wait from the added latency.

## The setting

- The same keys as the local game (`settings/framegen_settings`: `framegen.preset`, Custom's
  detail keys), on the setting layers at Global and Game; a remote game's Game layer is its own
  id (`lan:<peer>:<game>`). Off by default.
- A new key, `remote.stream-rate` ("Stream rate": the game's own, or 30 fps), at Global and
  Game, default the game's own. It is shown beside frame generation for a remote game only.
- `RemoteFrameGen` resolves the game's values through `resolveFrameGen` when the session starts
  and when a value changes; `GamePresentation` keeps the local Gamescope atoms and does not see
  remote games.
- The rows show for a remote game where the guest can generate (a Vulkan device with the needed
  features, PyroWave in use) and the host answered with modules; otherwise the heading says why.

## The host's rate cap

- The guest sends the rate it wants with its receive report (`wire::Report`), 0 for the game's
  own. The host's `HostStream` keeps it.
- `HostVideo` codes each frame once per codec and rate in use: a group at 30 takes a capture only
  when at least 1/30 s (less half a 60 Hz frame) has passed since its last one, by capture time.
  PyroWave is intra only, so any subset of frames decodes alone; a hardware codec's group at 30 is
  an encoder of its own at 30 fps.
- A frame's byte budget stays what it is at 60 (`frameBytes`), so the link's plan and the encode
  work halve at 30; `LinkRate` plans in bits per second at the stream's rate.

## Order

1. This plan.
2. The generator moved out of `src/FrameGen/` into the fork's `framegen/` library, behaviour
   unchanged; proven by the fork's headless frame generation run before and after.
3. Linux guest, stage A, behind the setting, with the host's 30 fps cap; proven with a hidden host
   and a hidden guest on one machine on a light game: about 60 presented frames a second from a
   30 fps stream, generated frames different from both neighbours, the added latency measured.
4. Linux guest, stage B (zero copy).
5. Android guest: MediaCodec's output as an `AHardwareBuffer` (an `AImageReader` surface instead
   of ByteBuffers), imported into the generator's Vulkan device
   (`VK_ANDROID_external_memory_android_hardware_buffer`, a YCbCr conversion sampler), the outputs
   handed to the shell's SDL GPU device the same way (needs the SDL fork's import on Android), the
   display's refresh from the window, the generation timed on the tablet's Mali or Kirin GPU to fit
   its refresh. The modules come from the host exactly as on Linux.
6. Windows guest: the same library on Vulkan with PyroWave; the hand-off to the shell through
   `VK_KHR_external_memory_win32` into SDL GPU's Vulkan device (fork), or stage A's copies first.

## Status

- Step 1: this file (2026-11-11).
- Step 2: done in fork `78dd9fb3` (branch `opensu`, pushed); openSU pins it from `24f646aa`, with
  step 3 its first consumer. `framegen/include/framegen/` is the public part
  (`Device.h`: `DeviceDesc_t`, `IQueue`, `CDevice`; `FramePacer.h`; `LosslessGenerator.h`:
  `CLosslessGenerator::Create`, `BeginSession`, `AddRealFrame`, `Generate` with output hooks,
  each returning its `Submission_t`), the rest is private to `framegen/src/`. The standalone
  CMake build installs `libframegen.a`, the headers and `framegen::framegen`.
  Proof (headless Gamescope 1280x720 at 60 Hz, ffplay's 32 px bar at 16 px per frame and 30 fps,
  fixed 2x, the player's DLL 3.2.2.0 balanced FP16; frames from Gamescope's PipeWire stream,
  `scratch/remote-framegen/tools/gs_proof.py`, runs in `scratch/remote-framegen/runs/`): the pin's
  binary (`gs-before-2x*`) and the new one (`gs-after-2x*`) both show 60 shown/s from 30 real/s,
  every captured frame one full 32 px bar with no half-bright edge, advancing 8 px per shown frame
  (the in-between at the half step); the linear blend on the new binary (`gs-after-blend`) shows
  half-bright edges on every other frame and Off (`gs-after-off`) 16 px steps at 30 distinct/s.
  Both binaries settle sometimes on a phase where every shown frame is generated (t 0.25 and 0.75:
  60 generated/s, `gs-before-2x-r3`, `gs-after-2x-r4`), which is the pacer's, unchanged by the move.

- Step 3: built against the fork at `34bab3a8` (openSU pin `eef48b2f`): the wire report carries
  the guest's wanted rate, `HostVideo` codes each codec group at the highest rate any of its
  guests wants (all frames if one wants the game's own), `/lan/framegen/modules` serves the
  modules to paired guests, `RemoteFrameGen` (`remote_frame_gen_vulkan.cpp`, a stub elsewhere)
  generates on the guest and `ShellWindow` presents at the display's refresh while it does. A real
  frame's arrival is when it reaches the generator, after decode. Proof (hidden host in the
  prefix's Gamescope, hidden guest, the scrolling NES ROM, stream rate 30, fixed 2x,
  `scratch/remote-framegen/tools/remote_fg.py`): 30.0 real/s, 59.8 to 59.96 shown/s, 420 generated
  in 14 s, capture to submit p50 46.9 to 48.1 ms against 14.9 to 17.3 ms off (about 30 ms added,
  the held real interval); the `held` runs, 40.3 ms on against 6.8 off. With the ROM's `held`
  motion (8 px every other game frame) every real frame lies on an 8 px lattice and a 2x
  in-between belongs 4 px off it. Unattended, the guest's `framegen-pace` log (run `h-pace`) has
  412 of 457 in-betweens at t 0.50, the rest around the 5 of 458 capture gaps that were not two
  game frames, and presents every 16.7 ms on their vblank.
- Grabbing the guest's frames (`/frame.png`) stalled its loop while the loop encoded the PNG:
  presents fell to about 21 a second and the pacer timed each in-between to those late vblanks
  (t 0.06 to 0.76, run `h-pace-grab`, 67 of 289 frames on the lattice). The loop now hands over
  pixels and the request's thread encodes; with grabs, presents stay at 60 a second, every
  in-between is at t 0.50 and 264 of 284 grabbed frames are a real frame on the lattice (116) or
  an in-between 4 px off it (148) (run `h-grab2`).
- The ghost is Lossless's own: 300 in-betweens read back from the generator beside their two real
  frames (run `h-dump`, a debug patch never committed) are 257 clean, 27 with the band wrapping at
  the picture's edge in the real frames too, and 8 with a faint curved streak beside a band in the
  right place and width (46 to 58 of 255 off the band, 0 in both real frames), once a scroll cycle
  at the same place; the other 8 flagged were the dump reading a real frame's slot after it was
  reused. So the LSFG 3 shaders draw it on a hard white edge over black, and the guest's copy,
  upload and draw add nothing; openSU has no fix for it. Grabbed frames' split bands were the edge
  wrap.
- Before the grabs in `h-grab2`, the pacer held a phase where every present is generated
  (t 0.19 and 0.69) for 8 s, as Gamescope's runs did. Replaying the run's logged arrivals and
  vblanks through `CFramePacer` (`scratch/remote-framegen/pacer-harness/`, 818 of 853 plans as
  logged, all 458 of `h-pace`) shows why: one skipped capture (a 50 ms interval at real frame 8)
  holds the 15-interval mean high, the timeline runs about 5 ms ahead of the arrivals, and when
  that interval leaves the history (frame 23) the aligned delay moves 1.7 ms, past the 5%
  tolerance, so `PlanOnVBlanks` keeps the old delay while the timeline settles off the vblank grid,
  until the delay stops fitting 275 real frames later. No judder (content steps 16.7 ms each
  present) and content 6.6 ms newer than aligned, but no real frame shows. Retaking the aligned
  delay once it has held still for 15 real frames cuts that run's offset plans from 318 to 58 with
  no more delay jumps (7 against 9) and leaves `h-pace` unchanged. That is now the fork's pacer
  (Gamescope `aa53d77`, its replay test `framegen/tests/FramePacerReplay.cpp` on both runs'
  arrivals). On the guest with the host stopped 50 ms every 5 s (`h-pin`), every gap is back at
  t 0.50 within 15 to 23 real frames and the longest run of plans off the half step is 20,
  against 308 real frames and a 276-plan run on the old pacer.

## Left

- The decode thread waking the generator, to remove the 4 to 13 ms a decoded frame waits for the
  loop pass ("Pacing and latency"); proven by that wait gone, measured as in `h-pace`.
- Generator settings against the Lossless streak: Custom's detail keys tried on `h-dump`'s scroll,
  kept only if a setting removes the 8 streaked in-betweens without new artefacts.
- Step 4, Linux stage B (zero copy).
- Step 5, the Android guest through `AHardwareBuffer`, timed on the tablet's GPU.
- Step 6, the Windows guest.

## Quality

Asked for 2026-11-12: openSU's frame generation measurably better than Lossless Scaling's.

### The harness

`tools/framegen_quality` (`uv run --frozen --group framegen python -m tools.framegen_quality
--scenes FILE`): each scene is recorded once at the full rate by a hidden `opensu-retro
--frame-pngs` (1280x720, its own home, presses over its control channel), every other frame is
left out, `opensu-framegen-pairs` makes it again from the real frames on either side through
`frame_gen::Interpolator` (the same `GeneratorDevice`, images and Lossless generator remote play's
guest runs), and each made frame is scored against the real one: PSNR over RGB, SSIM over luma and
NVIDIA's FLIP (lower is better). Those score a frame alone and charge a missing shape less than a
misplaced one, so a generator that erases what it cannot match scores well while the shape
flickers at half the rate on screen; two more scores see it: detail lost (the real frame's luma
edge energy the made one lacks) and flicker (the edge energy both real neighbours have around a
place, spread 6 pixels, that the made frame lacks; `repeat` is 0 by construction and pays in FLIP).
`blend` (the two real frames mixed) and `repeat` (the earlier one) are references. A scene's
`pixel` is the output pixels one source pixel covers, measured from its recording and passed as
`--source-pixel`. Each run's `build.json` names the generator binary's sha256, the source tree,
commit and a hash of its diff; the run is refused when the binary is older than any file under
`src/frame_gen`. `--rescore` scores a run's made frames again. Tables are over moving frames (the real frames on either side differ); runs,
scores and the worst frames side by side (real before, left out, after; each generator's frame
over its FLIP map) are in `scratch/framegen-quality/runs/<run>/`. Scenes are the player's own
cores and games, named in a JSON file outside the tree.

### Baseline and the choice

Lossless's LSFG 3 shaders (the player's DLL 3.2.2.0, balanced, FP16, flow scale 1) against the
references, run `base1`: on every scene with real motion Lossless is far ahead of `blend` and
`repeat` (Super Metroid's intro FLIP 0.038 against 0.086 and 0.093). Where a game moves only every
other frame (the scroll test ROM, which misses a vblank each frame and so moves every second one),
the frame left out equals a real one and `repeat` is perfect while every interpolator is not.

Of the three ways forward, openSU's own generator (d) came first: Lossless takes no motion
vectors and only interpolates (`Generate` takes t in (0, 1)), so host-supplied vectors (b) and
extrapolation (c) both need a generator of openSU's own anyway; PyroWave is intra only, so the
host has no encoder vectors to send, only a second motion search of its own. (d) also removes the
dependency on the player's DLL. FSR 3's interpolator needs the game's motion vectors and depth,
which neither a libretro core nor a stream has; its optical flow (MIT at FidelityFX SDK `v1.1.4`,
`docs/prior-art.md`) is the candidate to port if this generator's search falls short.

### openSU's generator (`flow`)

`src/frame_gen/flow_interpolator.cpp`, `shaders/flow_*.comp`, written from scratch: each real
frame's luma in a 4-level pyramid; on its arrival the motion from the earlier real frame, one
vector per 8x8 block of the frame between, searched symmetrically by the sum of absolute luma
differences less their mean (so a fade moves nothing), every vector within 8 pixels at the
coarsest level (64 in the frame) and each finer level's parents' 3x3 doubled and refined by 2; a
block either frame is flat over (a fade from white) keeps no motion. For each frame between,
`flow_choose` gives each pixel, of its block's and the 8 neighbours' vectors, the one whose two
real pixels agree best over 3x3, and marks the real pixels it takes; `flow_make` mixes the two
frames there by t. The share of a vector each frame moves is counted in the content's own pixels
(`SourcePixel`, the output pixels one source pixel covers) and rounded toward zero, so a scaled
picture moves in whole source pixels and stays sharp; a zoom or turn smaller than a source pixel
shows the earlier frame's scale until it reaches one. Real pixels no chosen motion took are what no vector fits (an
animating sprite, rain, a cut) or what only one frame shows (uncovered or about to be covered): a
shape of them (18 of a 5x5) in the nearer frame is shown in place and whole, lit for the fade; a
pixel that matched badly elsewhere takes, of its own and 8 neighbouring choices 4 pixels away, the
vector landing on untaken pixels of one frame, from that frame alone; specks both frames have
(rain) come from each weighted by t.

Runs `f9` (Lossless) and `p5` (`flow`), the same recorded frames for both, 119 frames left out per scene, moving frames:

| scene | PSNR L / flow (dB) | worst 5% | FLIP L / flow | worst 5% | FLIP repeat / blend | detail lost L / flow | flicker L / flow |
|---|---|---|---|---|---|---|---|
| Super Metroid, rain over the ship | 26.58 / 25.84 | 22.24 / 20.91 | 0.0671 / 0.0702 | 0.173 / 0.177 | 0.0844 / 0.0821 | 0.292 / 0.274 | 0.0344 / 0.0021 |
| Super Metroid, cave scroll | 26.12 / 25.04 | 23.09 / 20.89 | 0.0693 / 0.0739 | 0.111 / 0.123 | 0.0967 / 0.0897 | 0.230 / 0.192 | 0.0181 / 0.0017 |
| Super Metroid, gameplay demo | 27.13 / 26.50 | 22.67 / 21.25 | 0.0513 / 0.0435 | 0.113 / 0.129 | 0.0872 / 0.0860 | 0.073 / 0.045 | 0.0103 / 0.0044 |
| Super Metroid, intro text and fades | 29.64 / 29.19 | 24.44 / 25.46 | 0.0381 / 0.0285 | 0.075 / 0.052 | 0.0934 / 0.0857 | 0.185 / 0.119 | 0.0170 / 0.0090 |
| Chrono Trigger, title | 44.10 / 41.94 | 38.85 / 35.69 | 0.0017 / 0.0020 | 0.003 / 0.005 | 0.0035 / 0.0029 | 0.013 / 0.013 | 0.0008 / 0.0002 |
| Killer Instinct, title loop | 50.38 / 51.69 | 33.65 / 33.28 | 0.0080 / 0.0085 | 0.034 / 0.033 | 0.0132 / 0.0084 | 0.018 / 0.016 | 0.0001 / 0.0000 |
| Kirby & the Amazing Mirror, title fade | 36.78 / 36.43 | 25.62 / 24.64 | 0.0333 / 0.0328 | 0.082 / 0.083 | 0.0344 / 0.0329 | 0.027 / 0.014 | 0.0002 / 0.0000 |
| Monster in My Pocket, title | 45.17 / 44.43 | 40.92 / 41.55 | 0.0010 / 0.0011 | 0.002 / 0.002 | 0.0015 / 0.0009 | 0.017 / 0.012 | 0.0006 / 0.0000 |
| scroll test ROM (moves every other frame) | 23.07 / 23.45 | 22.93 / 22.05 | 0.0120 / 0.0133 | 0.012 / 0.015 | 0 / 0.0139 | 0.701 / 0.635 | 0 / 0 |

`repeat` and `blend` beside them are the placement check: `repeat` has no flicker by construction
and `blend` keeps both frames' detail, so a generator counts as placing moving content only where
its FLIP is below theirs, which both are by a wide margin on the Super Metroid scenes and neither is on the
title screens, where little moves.

What it shows: `flow` keeps more of the real frame (detail lost lower on every scene but Chrono Trigger's title, where it is level) and
flickers less (Lossless's flicker on the rain and cave scenes is its smeared raindrops). By FLIP
it is ahead of Lossless on the intro (25% lower, worst 5% PSNR 1 dB higher) and the demo (15%),
where the scrolling background lands sharp on the real frame's place while Lossless blurs it over
a half step; level on Kirby and within 0.0005 on Killer Instinct and Monster in My Pocket; behind on the rain and cave
scenes (5% and 7%: the foreground rock, behind the rain layer that moves the other way, is matched
by the rain and held at the earlier frame's place), on Chrono Trigger's title (its logo zooms by
less than a source pixel per frame and is drawn at the earlier scale, worst 5% PSNR 2.7 dB lower)
and on the scroll ROM, where the truth is the later real frame and rounding toward zero picks the
earlier one; which of the two a game shows is not in its 30 fps frames (Super Metroid's matches
the earlier). In its worst frames (`p5/worst/`) a sprite that animated between the real frames is
shown whole in the nearer frame's pose where Lossless draws a ghost of both. On the Kirby fade
from white `flow` draws the logo and text clean where Lossless garbles the small print. It is not
yet better than Lossless on every scene.

Tried and dropped for content-pixel shares: moving a block rigidly only where all 9 neighbouring
vectors agree lost most of the gain (intro FLIP 0.037); gating on how gradually the vectors
differ left seams on the Chrono zoom.

### Where `flow` sits

Today `flow` runs only in the harness, on the runner's `--frame-pngs` output scaled to 1280x720.
Lossless runs locally in the Gamescope fork on the composited game window, after the runner's
shader presets, and on remote play on the guest, on the decoded stream, after the host's shaders
and the encoder. Taking the luma at each content pixel's centre is right for a frame scaled by
nearest neighbour, but a player's preset (CRT, scanlines, smoothing) changes what the output
pixels hold, so in the runner frame generation is to take the core's frame before shaders and the
remote host to send that frame.

On a core's own frame `flow` works on it scaled up nearest by `frame_gen::flowContentScale` (half
of `flowBlockPixels`, so its finest blocks cover about 2 content pixels): `flow_take` reads the
real frame at each scaled pixel's source pixel, the search and `flow_choose` run at the scaled
size with `SourcePixel` scaled to match, and `flow_make` makes only the centre of each scaled
pixel, written straight into the frame at the core's size. That frame then goes through the
runner's compositor and the player's shaders like a real one. `python -m
tools.framegen_quality.core_frames --frames DIR` checks the writing back loses nothing: on Super
Metroid's demo at 256x224 (`opensu-framegen-pairs --core-frames`) all 60 generated frames equal,
bit for bit, the centres of what `flow` makes from the same frames scaled up 4x beforehand. A
generated frame then costs 975 K invocations and 159 M texels read, against 1.84 M and 187 M for
the 4x frame it equals; the real frame's ingest is the 4x one's, 2.44 M invocations.

### In the product

`framegen.generator` picks Lossless Scaling (the default) or openSU. openSU's runs in the built-in
runner (`retro/frame_generation`) and on the remote play guest (`RemoteFrameGen`), never in
Gamescope. In the runner the core's frame goes to `flow` before the compositor, and each present
at the display's refresh shows the real frame or a generated one through the compositor and the
player's shaders. With `--frame-pngs` the pacer runs on the core's own clock and waits for each
generated frame, so writing the PNGs moves no times. Super Metroid (PAL, 50 fps) on a 60 Hz
display: 4560 core frames gave 5468 presents, 3851 of them generated. In a 30 s real-time hidden
walk 1500 core frames gave 1794 presents (59.8 a second), 1248 generated and 1 dropped; `flow`'s
ready need settles at 6.5 to 7.7 ms (its ingest and first generated frame are done 5.2 ms after
a real frame arrives, p50; p90 6.2, max 7.0, at the GPU's idle clock). Once the window of ready
times fills, a need that fell steps the held delay down once, where that saves at least half a
refresh and keeps a quarter refresh above the need (`k_flNeedFallStepRefreshes`,
`k_flNeedFallMarginRefreshes`). A real frame shows on average 38.4 ms after the core drew it over
the walk (42.5 with the delay held from the start). The delays that land real frames on vblanks
are a few ms apart (R/5 at 50 fps, R/3 at 45), so a run settles in whichever one its arrival phase
gives: over 64 phases of the same walk the replay puts the walk's average at 36.1 to 43.4 ms, and
the settled delay at 33.3 to 43.0. Without frame generation it shows right away. On the remote
play guest, Lossless and openSU's generator each show 60.0 frames a second from the host's 30
(19 s, TwoPlayers ROM, a hidden host in headless Gamescope); their needs settle at 3.1 and 1.9 ms
and capture to submit falls with them, from 50.5 to 34.2 ms and from 51.0 to 34.6 ms. On a busy desktop the Lossless
guest drops 0 to 52 generated frames a run with or without the need-fall step; the drops come from
stalls in the real frames' arrival (gaps of 50 to 117 ms), not from the pacer: the same runs'
arrivals and ready times replayed through either pacer over 64 phases drop the same or fewer with
the step, and miss no generated frame.

`tests/frame_gen`'s replay runs the fork's pacer with `ReadyNeed` over 64 arrival phases of eight
paces, each real frame's first generated frame ready after one of `flow`'s 2844 measured times
(and twice them): no generated frame misses in any phase, the held delay steps at most once a
minute more than with a refresh's need, real frames show 7.1 to 8.4 ms sooner (2.0 to 3.2 at
twice the times), and in every phase the settled delay is the one a run planned for the settled
need from the start holds, less than half a refresh above it, or that run's delay sits inside
the quarter refresh a step keeps above the need and the arrival lag. With a need of 0 instead, 45 fps to 60 missed up to 22% of its generated frames
at the worst phase.

### GPU cost

`opensu-framegen-pairs` ends with `[perf]` lines (`frame_gen::GpuSpans`, compared with
`perf_compare.py`), per real frame (`ingest`) and per generated frame (`generate`). For `flow`:
dispatches, shader invocations, the bytes of the images each dispatch binds (a footprint, each
image once) and the texels its shaders read, counted by the shaders themselves while measuring
(an atomic per read behind a specialization constant, off in the product), and apart from them
the values they read from shared memory, so a read moved there is not counted as free. These
repeat exactly
and are the cost verdict. Both generators' GPU time comes from timestamps through the fork's
`IQueue` timing hooks; Lossless records its dispatches inside the fork, so only its time is
seen.

Super Metroid's demo, 60 generated frames:

| size | generate: invocations, texels read | ingest: invocations, texels read, shared values read |
|---|---|---|
| 256x224 | 115 K, 11.9 M | 153 K, 1.38 M, 9.95 M |
| 256x224 core frames, at 4x (the runner) | 975 K, 159 M | 2.44 M, 21.5 M, 147 M |
| 1280x720 | 1.84 M, 188 M | 2.45 M, 19.1 M, 117 M |
| 3840x2160 | 16.6 M, 1.68 G | 22.0 M, 155 M, 805 M |

The search gives each block a workgroup of 64 threads that try its candidates side by side, each
candidate's cost summed by one thread in the same order, and a reduction in shared memory picks
the lowest, the earlier candidate of equal costs and the zero vector before all, as the loop did
when one thread per block tried them in turn; its frames are bit for bit the loop's on every
scene (`--same-as`). The windows each set of candidates reads (the coarsest level's 289 vectors,
a finer level's 25 around its best parent, and the zero vector's) are loaded once per block into
a shared tile of at most 20 x 20 per frame, and the costs read them there, the frames again bit
for bit the same. A parent candidate equal to an earlier one or to the zero vector, and a finer
candidate equal to any of them, is not costed: of equal costs the earlier is kept, so the frames
are again the same. At 1280x720 that leaves 21 texels per frame pixel, most of them the parent
candidates left, which lie anywhere and are read from the frames. The loops over a window are
unrolled, each sum in its order, so a thread's reads go out together rather than one at a time:
the frames are bit for bit the same, and the search shader takes 168 VGPRs, 6 subgroups per SIMD.
The finer levels, with 9 and then 25 candidates, give each block 32 threads (one subgroup) and
tiles reaching 2 pixels past its window: 192 VGPRs and 5 blocks per SIMD against 3 at 64 threads,
the frames bit for bit the same. In the runner (Super Metroid from boot, 1500 frames at 1280x720,
three runs each, interleaved) the first frame between is ready 3.91 ms after its real frame at p50
(spread 0.04) and 4.47 at p90 (0.04), against 5.11 (0.14) and 6.33 (0.76) at 64 threads; the
smaller tiles alone at 64 threads gave 4.73 (0.09) and 5.63 (0.18). Two blocks to a 64-thread
workgroup took 256 VGPRs and 4 subgroups per SIMD; keeping a block's slot uniform for the compiler
needs a subgroup broadcast, which needs SPIR-V 1.3.
On the core frames at 4x, 60 frames at the GPU's idle clock, the search's levels take 3.7, 1.0,
0.28 and 0.13 ms, finest first (9.7, 2.9, 1.0 and 0.83 with the loops rolled); at the finest, its
phases (tiles, the flat and zero checks on 3 threads, the 9 parent candidates, the 25 around the
best) each keep most of the workgroup's 64 threads idle. Frames at 4x hold one content pixel per
4 x 4, but no two candidates read the same values in the same order, so none of the finest
level's costs repeat. The generated frame reads about 100 texels per pixel (each pixel's 9
candidate vectors over 3 x 3 in both frames, and the counts of untaken pixels around it), so at
4K it costs more than the search.

Searching on the content's own grid (the luma at each content pixel's centre, the pyramid and
search at the core's size, vectors scaled back for the frames between) cut the search to 23 M
texels at 1280x720 and 30 M at 4K, but lost on the worst frames: with 8 or 4 content pixel
blocks (30 or 15 frame pixels) the field is too coarse for the edges of the ship and sprites,
which break into scattered pixels (rain 125, demo 127), a dark smear sits over the Nintendo logo
(intro 55), and flicker on the rain, cave and intro scenes is 2 to 3 times higher, though FLIP
was lower on five scenes. It is not in.

Timestamps, a footnote: they move with the GPU's clocks, which drop for light work the tool waits
on, by up to 2.7x between runs and 3x for an unchanged pass. They put the loop's search at 17 to
28 ms per real frame at 1280x720 and 59 to 67 ms at 4K, the side-by-side search at 5.8 to 6.7 and
41 ms, Lossless's ingest at 0.44 to 0.99 and 5.6 ms, and `flow`'s generated frame under
Lossless's at every size.

### Left

- The search's cost (19 M texels per real frame at 1280x720, 155 M at 4K): the parent
  candidates' windows, often the same vector for neighbouring blocks, shared between blocks; the
  finest level's phases still leave most of its 32 threads idle. The generated frame's 100 texels
  per pixel.
- `flow` in Gamescope: Gamescope's generator is Lossless's; a game outside the built-in runner
  with openSU chosen gets no frame generation (the summary's note says where openSU's runs).
- The source pixel on remote play: `RemoteFrameGen` runs `flow` at scale 1 with a 1 x 1 pixel.
  Tried and not in: a datagram kind of its own (older guests drop an unknown kind unopened)
  carrying a versioned core frame size, a later version's fields skipped; the host sending it only
  for a built-in runner's frame streamed at its own size with no filter or shader preset; the
  guest taking it only for frames of that size (frames still on the way at the window's size
  ignore it) and running `flow` at `flowContentScale` there. On the guest's own decoded frames,
  every other one made again, moving frames only, two step offsets: Super Metroid's intro and
  title (1542 frames) gained 0.8 to 1.0 dB on PSNR p5 against a spread of 0.66 between the
  offsets, FLIP p95 0.0017 against a spread of 0.0025, worst frames equal (FLIP max 0.194 and
  0.162 either way) and the mean 0.02 to 0.05 dB lower; the NES test ROM (803 frames) lost 0.34
  dB and broke a growing 1-pixel bar's tip. Looked at: on the cut from black text to the machine
  both fail, scale 1 smearing black over the machine and scale 4 laying the text over it; on red
  text over black scale 4 keeps the letters whole; on the title the two differ below what shows at
  2x. It costs the ready need 4.4 ms (6.3 against 1.9 on the NES stream) and capture to submit
  about 7 ms (42.0 against 34.6). Worth trying again with a cheaper ingest at scale 4, or on
  content where p5 and the worst frames clear the spread clearly.
- Layers moving different ways in one block (rain over the cave's rock): one vector per block
  follows the busier layer. Settling each pixel's vector with its neighbours' (a cost per
  disagreeing neighbour) changed no score by more than 1%; carrying untaken pixels along a vector
  of their own tore sprites apart, because the untaken set of an animating sprite is itself patchy.
  A second vector per block, chosen by the residual the pair leaves, is not built: offline, on
  the finest fields of the rain, cave, demo and intro scenes at both step offsets (40 pairs each),
  each pixel reconstructed as the mean of its two moved pixels at t 0.5 and scored against the
  recorded frame between, adding the pair's 3 strongest global translations to `flow_choose`'s 9
  candidates lowered PSNR p5 on rain (-0.77 and -0.12 dB) and cave (-0.11 and -0.17) and the mean
  on both by 0.4 to 0.5 dB; the controls' p5 moved by 0.05 dB at most and their mean by 0.16 at
  most. On rain and cave, picked against the true frame, the same candidates gain 1.1 to 3.3 dB p5
  over the best of the 9, and the best of the 9 is 1.5 to 3.0 dB above what the agreement test
  picks. So the vectors are there and the selection loses them:
  the next cause to trace is `flow_choose.comp`'s 3 x 3 mean-free agreement test, which takes a
  vector that matches the two real frames where it does not match the frame between. The test
  stood in global translations for per-block second vectors, which fits rain's near-global fall;
  it does not show per-block seconds cannot help once the selection is right.
  Traced: the offline copy of `flow_choose`'s rule matches the product's choice image on every
  pixel of rain and cave to float32 rounding (1.5 to 1.7% of pixels are ties, nearly all at zero
  cost, and the copy breaks them its own way). On the worst 5% of pixels against the best of the
  9, the picked vector makes the two real frames agree exactly on the 3 x 3 once its mean is
  removed on 61 to 78%, and the right vector ties with it at zero on 19 to 31%: the test sees only
  the pixel's patch, so a flat patch or one off by a constant cannot tell vectors apart, and the
  right vector pays where another layer differs along it. Tried on the same 9 candidates and not
  in, PSNR p5 against the copy of today's rule at both step offsets: the 3 x 3 without its mean
  +0.28 and +0.24 dB on rain (offset spread 0.71), +0.24 and +0.06 on cave (0.19), -0.54 and
  -0.04 on the demo (0.91); the mean over the block's 12 x 12 window in place of the patch's +0.27
  and +0.02 on rain, -0.01 and 0.00 on cave; ties broken by the lowest difference over that
  window +0.12 and -0.05 on rain, -0.04 and -0.07 on cave; the two together as the block mean.
  None clears the spread on rain and cave. The best of the 9 is 1.5 to 3.0 dB p5 above today's
  rule, and none of the patch- or block-local rules tried comes near it, so that bound is not
  reachable by a rule of this kind. Caveats: the made pixel is the plain mean of the two moved
  pixels, not `flow_make`'s; two step offsets give a coarse spread; the copy's own tie-breaking
  moves 1.5 to 1.7% of pixels. The run's numbers are in `scratch/framegen-quality/layers/out1/` and
  `out2/` (`step1.py`, `step2.py`), temporary.
- (b) and (c) on top of `flow`; the shell's own animated screens as scenes (they have no per-frame
  dump yet).

## Found while planning step 3

- openSU's PyroWave decoder runs on a Vulkan device of its own (`pyrowave_create_default_device`)
  and reads every picture back to CPU RGBA (`decode_cpu_buffer_synchronous`); the shell draws it
  on SDL GPU's device. So the decoded frame is not yet a Vulkan image the guest can use: stage A
  uploads it, and stage B decodes with PyroWave's direct API on the generator's device.
- The generator's device needs `shaderFloat16` (else FP32 modules), `shaderStorageImageWriteWithoutFormat`,
  `shaderStorageImageExtendedFormats` and robustness2's `nullDescriptor` (missing inputs are null
  views), on a compute queue; the runner's `retro::VulkanDevice` is the libretro core's device and
  enables none of them, so the guest makes its own.
- The loader of shader modules reads them from a DLL path (`LoadLosslessShaders`); the guest needs
  the same checks from modules held in memory, a public loader in `framegen/` beside it, and the
  host the resource reader to send them (`PeResources`, private today).
