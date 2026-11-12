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
  60), the largest recent arrival lag, up to one refresh, and the generation itself (2.2 ms per
  generated 720p frame balanced on an RX 6700 XT, more on a phone GPU, which must fit it inside
  its refresh). Stage A adds an upload per real frame and a readback per shown frame.
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
`blend` (the two real frames mixed) and `repeat` (the earlier one) are references. `--rescore`
scores a run's made frames again. Tables are over moving frames (the real frames on either side differ); runs,
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
frames there by t, bilinearly. Real pixels no chosen motion took are what no vector fits (an
animating sprite, rain, a cut) or what only one frame shows (uncovered or about to be covered): a
shape of them (18 of a 5x5) in the nearer frame is shown in place and whole, lit for the fade; a
pixel that matched badly elsewhere takes, of its own and 8 neighbouring choices 4 pixels away, the
vector landing on untaken pixels of one frame, from that frame alone; specks both frames have
(rain) come from each weighted by t.

Runs `f9` (Lossless) and `o1` (`flow`), the same recorded frames for both, 119 frames left out per scene, moving frames:

| scene | PSNR L / flow (dB) | worst 5% | FLIP L / flow | worst 5% | FLIP repeat / blend | detail lost L / flow | flicker L / flow |
|---|---|---|---|---|---|---|---|
| Super Metroid, rain over the ship | 26.58 / 25.77 | 22.24 / 21.37 | 0.0671 / 0.0724 | 0.173 / 0.175 | 0.0844 / 0.0821 | 0.292 / 0.275 | 0.0344 / 0.0017 |
| Super Metroid, cave scroll | 26.12 / 25.11 | 23.09 / 21.68 | 0.0693 / 0.0751 | 0.111 / 0.125 | 0.0967 / 0.0897 | 0.230 / 0.187 | 0.0181 / 0.0013 |
| Super Metroid, gameplay demo | 27.13 / 26.36 | 22.67 / 21.70 | 0.0513 / 0.0519 | 0.113 / 0.116 | 0.0872 / 0.0860 | 0.073 / 0.054 | 0.0103 / 0.0049 |
| Super Metroid, intro text and fades | 29.64 / 29.33 | 24.44 / 23.83 | 0.0381 / 0.0373 | 0.075 / 0.074 | 0.0934 / 0.0857 | 0.185 / 0.162 | 0.0170 / 0.0103 |
| Chrono Trigger, title | 44.10 / 43.42 | 38.85 / 38.42 | 0.0017 / 0.0017 | 0.003 / 0.003 | 0.0035 / 0.0029 | 0.013 / 0.009 | 0.0008 / 0.0002 |
| Killer Instinct, title loop | 50.38 / 52.00 | 33.65 / 33.46 | 0.0080 / 0.0084 | 0.034 / 0.033 | 0.0132 / 0.0084 | 0.018 / 0.015 | 0.0001 / 0.0000 |
| Kirby & the Amazing Mirror, title fade | 36.78 / 36.50 | 25.62 / 24.61 | 0.0333 / 0.0327 | 0.082 / 0.082 | 0.0344 / 0.0329 | 0.027 / 0.014 | 0.0002 / 0.0000 |
| Monster in My Pocket, title | 45.17 / 44.75 | 40.92 / 41.39 | 0.0010 / 0.0010 | 0.002 / 0.002 | 0.0015 / 0.0009 | 0.017 / 0.012 | 0.0006 / 0.0000 |
| scroll test ROM (moves every other frame) | 23.07 / 24.06 | 22.93 / 22.95 | 0.0120 / 0.0128 | 0.012 / 0.014 | 0 / 0.0139 | 0.701 / 0.605 | 0 / 0 |

`repeat` and `blend` beside them are the placement check: `repeat` has no flicker by construction
and `blend` keeps both frames' detail, so a generator counts as placing moving content only where
its FLIP is below theirs, which both are by a wide margin on the Super Metroid scenes and neither is on the
title screens, where little moves.

What it shows: `flow` keeps more of the real frame (detail lost lower on every scene) and flickers
less (Lossless's flicker on the rain and cave scenes is its smeared raindrops). By FLIP it is ahead
of Lossless on the intro (2% lower), level on the demo and the title screens, and behind on the
rain and cave scenes (8%), with worst 5% PSNR up to 1.4 dB lower there. In its worst frames
(`o1/worst/`) a sprite that animated between the real frames is shown whole in the nearer frame's
pose where Lossless draws a ghost of both, and in the cave the foreground rock, behind the rain
layer that moves the other way, is matched by the rain and held at the earlier frame's place. On
the Kirby fade from white `flow` draws the logo and text clean where Lossless garbles the small
print. It is not yet better than Lossless.

### Left

- `flow` in the product: a generator choice beside Lossless in `settings::framegen_settings`, taken
  by `RemoteFrameGen` (it already holds an `Interpolator`), and by the fork's Gamescope generator;
  until then it runs only in the harness. Its GPU time per generated frame is not measured yet.
- Odd displacements: a background that moves 1 pixel over two game frames is, in the truth, one
  of the two real frames; both generators draw the half step (detail lost 0.6 to 0.7 on the scroll
  ROM). The recorded frames are the runner's 1280x720 output, scaled 3.75 x 3.21 for the SNES, so
  rounding the shares to whole output pixels changes nothing measurable. Next: `Interpolator::begin`
  takes the content's source-pixel scale, which the runner and the remote host pass, and shares
  round in source pixels. The runner's later option is generating at the core's native size before
  scaling, which is also far cheaper.
- Layers moving different ways in one block (rain over the cave's rock): one vector per block
  follows the busier layer. Settling each pixel's vector with its neighbours' (a cost per
  disagreeing neighbour) changed no score by more than 1%; carrying untaken pixels along a vector
  of their own tore sprites apart, because the untaken set of an animating sprite is itself patchy.
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
