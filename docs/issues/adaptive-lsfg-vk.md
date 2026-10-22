# Adaptive frame generation through lsfg-vk

Status: open. Decided 2026-10-22: frame generation runs inside our Gamescope fork with our own MIT
generator (see "Decision"). Phases A (mechanism, linear-blend stand-in) and B (adaptive pacing) are
in the fork at `7b41ee21` and pinned; the generator and phase C (openSU UI) are not built.

The player: "Another request I have is adaptive LSFG-VK". lsfg-vk is the Vulkan layer that runs
Lossless Scaling's frame generation on Linux; the player has a fork (`SomeoneIsWorking/lsfg-vk`).
It needs the player's own `Lossless.dll` from their Lossless Scaling install on Steam, which is
never copied or shipped.

## Decision

The player chose the compositor-side route and then: "We can make our own LSFG-VK as MIT,
discarding the fork". So:

- Frame generation lives in the Gamescope fork (BSD-2, like the rest of Gamescope). Gamescope keeps
  the focused window's previous and current committed images, has a generator make the images in
  between at any timestamp in (0, 1), and shows them on its own vblank. Nothing per game process,
  no layer, no WSI or FIFO latch change.
- Nothing from lsfg-vk (GPL-3) goes into the fork, and the fork does not link it. The pushed
  `SomeoneIsWorking/lsfg-vk` commit `a7edf87` (branch `opensu`, per-frame timestamps in its
  backend) is not used.
- The generator is our own MIT code, written clean-room from a behaviour spec derived from the
  player's own `Lossless.dll` (Ghidra) and public docs, by an implementer who has not read lsfg-vk.
  It goes behind `IFrameSynthesizer::Generate(previous, current, t, output)`.

## Frame generation in the fork

The fork's `src/FrameGen/` (pinned `7b41ee21`):
- `FramePacer`: the one owner of how many images each real frame gets, their timestamps and when
  each is shown.
  - Base rate: the mean of the last 15 real-frame intervals; an interval 6x off the mean restarts
    the history. Lossless Scaling's `frametime_buffer_size` and
    `frametime_buffer_reset_multiplier` (see "Lossless Scaling's Adaptive mode").
  - Count and timestamps: a phase accumulator steps by `1 / ratio` (fixed) or
    `base rate / target` (adaptive, never below `1 / (max + 1)`), carried over between real
    frames, so fractional ratios work. An output within 0.05 of a real frame shows that real frame
    (`real_timestamp_tolerance`). When a carried phase would put more than `max` generated images
    in one interval, the phase realigns so the real frame is one of the outputs.
  - Adaptive decision: the target is the set rate, or the display's refresh when 0, and never above
    the refresh. Nothing is decided until the history is full. Generation stops once the base
    rate reaches `target - tolerance` and restarts only below `target - 1.5 * tolerance`, and
    either change needs the base rate past its threshold for 15 real frames in a row. Below
    10 fps (`base_framerate_threshold`) only real frames show, in both modes.
  - Presentation: an output shows one real interval after its content on a timeline of the real
    frames smoothed by 0.1 of each arrival's error, later by the largest arrival lag of the last
    15 frames and half a refresh, so the vblank it lands on is after its real frame arrived. At
    most one output per vblank, and an output whose vblank an earlier one already holds is not
    made. Without generation each real frame shows at the next vblank.
- `FrameGenerator`: copies each new commit of the focused window (8-bit SDR only) into one of
  three buffered frames, runs the synthesizer for the planned timestamps into that interval's set
  of images, and gives `paint_window` the image for the current vblank in place of the commit's
  texture. PipeWire follows it. Logs `Pacing <state> at real N` whenever the state (measuring,
  below the floor, at the target, within the tolerance, generating) changes.
- `LinearBlendSynthesizer`: the stand-in generator, `previous * (1 - t) + current * t` through
  Gamescope's own compositor. It moves nothing; it makes each image's timestamp measurable.
- Root atoms, all CARDINAL, set by openSU: `GAMESCOPE_FRAMEGEN` (0/1), `GAMESCOPE_FRAMEGEN_MODE`
  (0 fixed, 1 adaptive), `GAMESCOPE_FRAMEGEN_RATIO` (thousandths, default 2000),
  `GAMESCOPE_FRAMEGEN_TARGET` (thousandths of fps, 0 the display's refresh),
  `GAMESCOPE_FRAMEGEN_MAX` (generated per real frame, default 3, 1 to 15),
  `GAMESCOPE_FRAMEGEN_TOLERANCE` (thousandths of fps, default 0); `GAMESCOPE_FRAMEGEN_DLL`
  (string, the player's `Lossless.dll`, unused until the generator).

## Proof (2026-10-22)

Headless Gamescope 1280x720; the game is ffplay showing a 32 px bar that moves 16 px per frame (a
lavfi source at a fixed rate, or a pre-rendered file with variable timestamps), or vkcube through
the WSI layer capped by MangoHud, whose frames arrive on its own vsync (33/17 ms). Frames are
read from Gamescope's PipeWire stream (`scratch/fg-phaseA/`: `runb.sh`, `analyze.py`,
`matrixb.sh`). Per-run results are in `scratch/fg-phaseA/b_*`.

Adaptive:
- 43 fps, target 60, at 60 Hz: outputs per real frame 1, 2, 1, 2, 1 (mean 60/43); 60.0 shown/s,
  0 dropped; the bar advances 0.717 per shown frame (stdev 0.02); shown spacing 16.67 ms (stdev
  0.02 ffplay, 0.08 vkcube).
- 30 fps, target 60, max 1: 2 outputs per real frame (one real), 60 shown/s.
- 30 fps, target 120 at 120 Hz, max 2: 3 per real frame (one real), 90.0 shown/s, never more;
  ffplay and vkcube alike.
- Target 0 at 120 Hz, 30 fps, max 3: the target resolves to 120; 4 per real frame, 120.0 shown/s.
- 55 fps, target 60, tolerance 6: `Pacing within the tolerance`, no generated image.
- 50 to 62.5 fps (mean 56) with tolerance 6: within the tolerance for the whole run. With
  tolerance 4 the measured base crossed the stop threshold (56) 454 times in 1084 frames; it never
  fell below the restart threshold (54), so generation never started: one decision line.
- 30 and 50 fps alternating every 100 frames: the count follows over the 15-frame history. After a
  rise, outputs show up to one vblank late until the history catches up, and one generated image
  in 27 s was dropped; after a fall, about 5 fewer outputs a second for a second (holds with
  nothing due). Shown 55.5 to 60/s per 2 s window.
- 8 fps: `Pacing below the floor`, real frames only.

Fixed (phase A behaviour): 30 fps 2x and 40 fps 1.5x at 60 Hz, ffplay and vkcube, and 30 fps 4x at
120 Hz show the same steps (0.500, 0.667, 0.250), sequences and 60 or 120 shown/s as phase A.

Off (atoms unset, or `GAMESCOPE_FRAMEGEN=0`): byte-identical frames to the pin before frame
generation, `e297b648`. `VK_LAYER_KHRONOS_validation`: the on runs report only the IDs and counts
of the off run, which match that pin (dmabuf import and private pNext IDs at start-up).

Found while proving, all fixed in `FramePacer` and `FrameGenerator`:
- Output times taken from raw arrivals put an image before its own real frame when arrivals are
  vsync-quantized and timestamps small, so it missed its vblank: hence the arrival-lag delay.
- Two buffered frames reclaimed an interval's images while they were still queued once the
  delay passed one interval: hence three.
- A carried phase could leave every output between real frames and the maximum then discarded
  one: hence the realignment.
- While the history lags a rising rate the plan asked for more outputs than vblanks and they piled
  up late: hence one claimed vblank per output.

Not proven: DRM and nested (Wayland) backends; HDR and 10-bit content (refused, shown unchanged);
windows with non-opaque alpha (the blend assumes opaque); the real generator; a game in openSU.
Under heavy machine load the sources stall and burst; the history then restarts and real frames
show until it refills, which the log shows as `Pacing measuring`.

## The bar

From the player, the bar for adaptive:

- A target frame rate, with "the display's refresh rate" as one choice.
- A maximum number of generated frames per real frame.
- Output never exceeds either limit.
- A tolerance: a game close to the target generates nothing (55 fps against a 60 target with
  enough tolerance shows the real frames only).
- "Being adaptive is very important. I don't want it if it can't be FULLY adaptive." Fixed
  multipliers alone do not count. The generated count changes from one real frame to the next
  (fractional ratios like 60/43), and output is paced to the target.

## Verdict

- Upstream lsfg-vk 2.0.0: **no**. It has fixed integer multipliers only and one pacing mode
  (`vsync`). Adaptive is listed as planned, with no date. Its licence forbids derivative works, so
  we cannot build on it.
- The fork's Vulkan layer (C++): **no**. Its multiplier is a fixed unsigned integer, every real
  frame gets exactly `multiplier - 1` generated frames at timestamps `(i+1)/multiplier`, and they
  are presented back to back under forced FIFO.
- The fork's C# `LsfgFramePacer` (used only by Kenji-NX's own LSFG chain): **partly**. It varies
  the count per real frame with fractional credit against a target, under a ceiling. It has no
  tolerance setting, spaces timestamps evenly within each real frame rather than carrying the
  phase, and does not pace presents.
- Feasible in the fork: **yes**, and the generator already takes any timestamp. The effort is
  several weeks, not days. Most of it is presenting generated frames on time from inside the
  game's process, and qualifying that under our Gamescope (see "What adaptive takes").

## lsfg-vk upstream

- **Where it lives.** GitHub `PancakeTAS/lsfg-vk` was emptied on 2026-09-05. Its default branch
  `migration` holds only a README pointing to [lsfg-vk.dev](https://lsfg-vk.dev). Source is
  self-hosted at `git.lsfg-vk.dev/lsfg-vk`, and v1 is archived at `git.lsfg-vk.dev/lsfg-vk-archive`.
  GitHub issues are disabled; issues go to Codeberg and Discord
  ([announcement](https://lsfg-vk.dev/blog/important-changes-to-lsfg-vk/)).
- **Licence.** v1 was MIT and the v2 development tree was GPL-3.0-or-later. The released v2 is
  [CC BY-NC-ND 4.0](https://lsfg-vk.dev/blog/important-changes-to-lsfg-vk/): no commercial use
  and no derivative works. GPL code published before the change stays GPL, which covers the fork.
  Nothing later can be merged into it.
- **Current release.** [v2.0.0](https://lsfg-vk.dev/blog/release-v2.0.0/) (2026-09-05; on GitHub,
  `v2.0.0-dev` "dev29" of 2026-08-27 and v1.0.0 of 2025-07-30). It adds a bindless pipeline with
  FP16 (2-3x on 2:1 FP16 hardware), less VRAM, a 32-bit layer, a Qt6 `lsfg-vk-ui`, and
  `lsfg-vk-cli` (benchmark, validate, healthcheck). It also adds process matching by Steam App ID.
  It needs the **`lsfg-vk` beta branch of Lossless Scaling on Steam**, which ships the shaders as
  `lsfg-vk.dll` with new resource ids (Kenji-NX `docs/project-state.md` S010).
- **Adaptive.** Not supported. "Adaptive frame generation" and dual-GPU are listed as planned on
  [lsfg-vk.dev](https://lsfg-vk.dev) and were left out of the release. The
  [pacing page](https://lsfg-vk.dev/docs/configuration/pacing-modes/) lists only `vsync`: frames
  are presented as soon as they are generated, and vsync has to be on.
- **Configuration** ([docs](https://lsfg-vk.dev/docs/configuration/)):
  - The file is TOML at `~/.config/lsfg-vk/conf.toml`. `XDG_CONFIG_HOME` and `LSFGVK_CONFIG`
    override the path. It holds `version = 2`, a `[global]` table (`dll`, `allow_fp16`,
    `log_level`, `log_file`) and `[[profile]]` tables.
  - Profile keys are `name`, `active_in`, `multiplier`, `flow_scale`, `performance_mode`,
    `pacing`, `override_present_mode` and `preserve_swapchain_image_count`
    ([options](https://lsfg-vk.dev/docs/configuration/configuration-options/)). `multiplier`,
    `flow_scale` and `performance_mode` reload while the game runs.
  - `active_in` is a string or a list, matched against the end of the Linux binary path, then the
    Windows `.exe` found in a Wine or Proton process's cmdline, then `/proc/self/comm`.
- **Environment** ([docs](https://lsfg-vk.dev/docs/configuration/environment-variables/)):
  - `DISABLE_LSFGVK` turns the layer off. `LSFGVK_CONFIG` names the file. `LSFGVK_PROFILE` picks a
    profile by name and overrides matching.
  - With `LSFGVK_ENV=1` no file is read. The layer then takes `LSFGVK_DLL_PATH`,
    `LSFGVK_MULTIPLIER`, `LSFGVK_FLOW_SCALE`, `LSFGVK_PERFORMANCE_MODE`, `LSFGVK_PACING_MODE`,
    `LSFGVK_NO_FP16` and the others from the environment.
- **Vulkan.** v2.0.0 needs Vulkan 1.2 (it was 1.3). The layer runs frame generation on its own
  `VkDevice` and shares images and semaphores with the game's device through
  `VK_KHR_external_memory_fd`, `VK_KHR_external_semaphore_fd` and `VK_KHR_timeline_semaphore`.
  It needs `VK_EXT_pci_bus_info` to pick the GPU.
- **Gamescope.** Its troubleshooting doc (fork `docs/Troubleshooting.md`, `docs/Configuration.md`)
  says Gamescope does not respect the forced V-Sync, which skips generated frames. The workaround
  is `ENABLE_GAMESCOPE_WSI=0` on Gamescope or Steam Deck, which pulls the Gamescope WSI layer out
  of the game. In openSU every game keeps that layer (`launch/child_environment` strips
  `DISABLE_GAMESCOPE_WSI`, which only the shell sets). So inside our nested Gamescope, the layer's
  extra presents go through the Gamescope WSI layer, and implicit-layer order decides which layer
  sees which present. Tested in step 1: the layer sits above Gamescope WSI, and Gamescope drops
  the generated commits (see "Step 1 result").

## The fork `SomeoneIsWorking/lsfg-vk`

- The fork is GPL-3.0. Its default branch is `develop` and it was last pushed on 2026-09-08. Its
  base is upstream `218820e` ("chore: Adjust to new compiler warnings", 2026-04-25), the
  GPL v2-dev tree. It shares no history with upstream's current `migration` branch, which was
  rebuilt from an orphan commit.
- `develop` adds 4 commits, and every one is C# or packaging. None touches the C++ layer or
  backend:
  - `287f4a2`: the `src/LsfgVk` .NET library, which holds the descriptor layouts of the 25 shader
    modules, module-set validation and `LsfgFramePacer`.
  - `a25d63d`: ignores .NET build outputs. That commit also checked in `bin/` and `obj/` files.
  - `8efd7ed`: a NuGet package `SomeoneIsWorking.LsfgVk` and a `dotnet-package.yml` workflow.
- There is a local checkout at `~/repo/shared/lsfg-vk`, with `origin` set to the fork and
  `upstream` to `PancakeTAS/lsfg-vk`. It is detached at `aacdd5f`, two unpushed commits ahead of
  `origin/develop`. Both are `LsfgFramePacer` fixes ("size the recovery window from the
  cadence", "stop discarding the deficit timer"). `~/repo/shared/lsfg-vk-csharp` (not a git
  repository) holds only `src/` and `tests/`.
- The fork's consumer is Kenji-NX (`src/Ryujinx.Graphics.Vulkan/Effects/LosslessFrameGenerationChain.cs`,
  `Window.cs`). Kenji-NX runs its own LSFG compute chain inside the emulator's renderer rather
  than the layer. It calls `LsfgFramePacer.Plan` once per real frame and presents that many
  generated frames back to back after the real one. Its pacer is a port of Eden's
  `FrameGenPacer`, which is prior art for this card.
- `LsfgFramePacer` against the bar:
  - It keeps a smoothed real-frame interval. The output credit grows by `interval x target` and
    yields `floor(credit) - 1` generations, capped by the configured maximum and a learned limit.
  - The limit starts at 0. It rises one step only after the target has gone unmet (below 95%)
    for a second, and it is probed back down when the native rate drops (load probing).
  - Below 10 fps, and after a stall, it pauses generation for a cadence-sized window.
  - Missing for the bar: a tolerance setting (once the limit is up, 55 fps against 60 still
    generates), a phase-continuous timestamp, and present pacing.
- The fork's layer finds `Lossless.dll` through `ls::findShaderDll`
  (`lsfg-vk-common/src/helpers/paths.cpp`). It looks under `$XDG_DATA_HOME` and then `$HOME`,
  in `<root>/steamapps/common/Lossless Scaling/Lossless.dll` for native Steam,
  `~/.steam/steam`, Debian, Flatpak and Snap. It also tries the working directory. It does not
  read other Steam libraries from `libraryfolders.vdf`. It parses the classic `Lossless.dll`
  (the default Steam branch) and not 2.0.0's `lsfg-vk.dll`.

## Lossless Scaling's Adaptive mode

Read from the player's own install, 3.2.2, Steam build 19655272, at
`~/.local/share/Steam/steamapps/common/Lossless Scaling`. The UI, `LosslessScaling.dll` (.NET),
was decompiled with `ilspycmd` 11.1, and the native engine `Lossless.dll` (D3D11 C++) with
Ghidra 12.0.4. Work files are in gitignored `scratch/lsfg-re/` and nothing was copied out.
The prior art, lsfg-vk's `docs/Journey.md`, reverse-engineered only the shaders and the dispatch
math. It covers nothing on pacing.

- **Settings.** The UI passes `ApplySettings(... frameGenerationMode, frameGenerationMultiplier,
  frameGenerationTarget, ..., queueTarget ...)`.
  - Mode is `FIXED` or `ADAPTIVE`. The multiplier is a float (1-20, so fixed 2.5x exists). The
    target is a float, 10-1000, defaulting to the rounded DWM refresh rate.
  - There is no cap on generated frames per real frame. The only limits are the target and the
    10 fps floor.
  - `config.ini` beside the exe holds engine tunables: `[capture] frametime_buffer_size = 15`,
    `frametime_buffer_reset_multiplier = 6`, `queue_draining_momentum = 0.01`, and
    `[lsfg] real_timestamp_tolerance = 0.05`, `base_framerate_threshold = 10`. The engine also
    reads an optional `[lsfg] multiplier` and `[rendering] flush`.
- **Base rate.** Every captured frame carries its time since the previous capture. The engine
  keeps a 15-entry ring of these and uses the mean as the base interval. A delta more than 6x
  above or below the mean restarts the ring. When more frames are queued than the queue target,
  each delta is shortened by 1%, which drains the queue.
- **Per-frame count.** The step is the length of one output frame, in units of one real frame.
  For adaptive it is `(1000 / target) / base interval in ms`, which is base rate / target. For
  fixed it is `1 / multiplier`.
  - A phase accumulator carries over between real frames. After each real frame the engine emits
    outputs at `phase += step` while `phase + step <= 1 + tolerance`, then subtracts 1.
  - The count per real frame is therefore whatever fits. At 43 to 60 the step is 0.717 and the
    count alternates 1, 2, 1, 2 with timestamps such as 0.717, 0.433/0.150, ...
  - Below `base_framerate_threshold` (an interval over 100 ms) the phase resets and only real
    frames show.
- **Timestamp.** Each output's timestamp is its fractional phase between the previous and the
  current real frame. The generator, `Lossless.dll` `0x180021940`, writes it into every pass's
  constant buffer at offset `0x1c`. That is the same `timestamp` field as lsfg-vk's
  `backend::ConstantBuffer`, so lsfg-vk's ported shaders take any timestamp.
  - A timestamp within `real_timestamp_tolerance` (0.05) of 0 or 1 shows the previous or current
    real frame instead of a generated one.
  - Lossless Scaling's tolerance is only this snap. At 55 against 60 the step is 0.917, and
    almost every output is still generated. The player's tolerance (pass through when close) is
    new behaviour, not Lossless Scaling's.
- **Pacing.** Presents are spaced by time, not by vblank feedback. Before each present,
  `0x180014b80` sleeps on a high-resolution waitable timer until shortly before one output
  period after the last present, then spins on `QueryPerformanceCounter`. The period is
  `1 / target` in adaptive and `base interval / multiplier` in fixed. Lossless Scaling can do
  this because it is a separate process presenting its own window, not the game's thread.
- **Function map.** `0x180014c90` is the per-capture loop, `0x180010f70` copies the settings into
  the scaler object, `0x180033c30` is the WGC capture with the frame-time ring and `0x18000ea50`
  takes the ring's mean.

## What adaptive takes in the fork

1. **Backend: per-frame timestamps.** The timestamps are baked into one constant buffer per
   destination when the context is created (`lsfg-vk-backend/src/helpers/utils.cpp`,
   `createCtx`). `scheduleFrames` always renders every destination.
   - Change: `scheduleFrames(ctx, timestamps)` writes each pass's timestamp and renders only
     `timestamps.size()` passes, with up to max-generations destinations allocated.
   - Size: small, a few hundred lines.
2. **Layer: the pacer.**
   - Measure the base interval from the game's own `vkQueuePresentKHR` calls (ring mean, outlier
     reset, as above).
   - Carry the phase and step at base / target.
   - Cap the count at max-generations, so output never goes past the target or the cap.
   - Snap near-real timestamps, stop below 10 fps, and add the player's tolerance: pass through
     while the base rate is within the tolerance of the target, with hysteresis so it does not
     flap.
   - New profile keys `mode`, `target` (a number, or the refresh), `max_generated` and
     `tolerance` in `config.cpp`.
   - Size: small to medium.
   - The rule would then exist twice, in C# for Kenji-NX and in C++ for the layer. That breaks
     "one implementation" across the two products, and needs a decision: accept it, or have
     Kenji-NX use the layer.
3. **Layer: present pacing.** This is the large part.
   - Today the layer presents its generated images inside the game's own present call, back to
     back.
   - Paced output needs the generated presents spread at 1/target. Blocking the game's present
     thread to do that lowers the game's own rate. The layer therefore needs its own present
     thread, which must synchronise access to the game's `VkQueue` (external sync) and the
     swapchain-image acquire.
   - Under FIFO with the target at the refresh, the queue paces by itself, but the timestamps
     then go stale by the queue's depth. Exact phase needs present feedback
     (`VK_KHR_present_id`/`present_wait`, or `VK_EXT_present_timing`) through Gamescope's WSI.
   - Size: 1-3 weeks with qualification.
4. **Gamescope.** In our fork of Gamescope, check that FIFO holds through its WSI for nested
   games, that present-wait or timing is exposed, and the order of the lsfg layer against the
   Gamescope WSI layer. Fix it in our Gamescope fork if needed. Step 1 answered the order and
   FIFO: the FIFO latch has to keep commit order (see "Step 1 result"); present-wait and timing
   are still unchecked.

An alternative, not a substitute: Lossless Scaling's own model is a compositor that captures the
game and paces its own output. openSU already owns a Gamescope fork that sees every game commit
and knows its vblank. Generating frames in Gamescope's compositor would pace against real vblank,
reach Steam, Wine and runner games with no layer injection, and take its per-game setting from an
openSU root atom.
- Cost: porting the lsfg-vk backend into Gamescope's Vulkan renderer, per-surface frame history,
  and the Gamescope fork becoming GPL-3 by linking it.
- Size: comparable to step 3, and it replaces steps 3-4 and the delivery work below.

## Step 1 result

Build-order step 1, run on 2026-10-22. Evidence and scripts are in gitignored
`scratch/lsfg-step1/`.

**Setup**
- Layer: the fork's `develop` at `8efd7ede48edc86966d9d3d2c9a1130977f13a80`, unmodified. It was
  cloned to `build/lsfg-vk-src` and built Release with Clang (`CMAKE_CXX_COMPILER_ID=Clang`) into
  `build/lsfg-vk/install`. Each run loads it through `VK_ADD_IMPLICIT_LAYER_PATH`.
- Config: `LSFGVK_CONFIG` points at `scratch/lsfg-step1/conf.toml`, with one profile
  (`active_in = 'vkcube'`, multiplier 2, flow scale 1.0, pacing `none`). Its `dll` names
  `~/.local/share/Steam/steamapps/common/Lossless Scaling/Lossless.dll` (default branch, build
  19655272), read in place. The DLL's SHA-256 is in `lossless-dll.sha256`.
- Gamescope: openSU's pinned fork `e297b648` on `--backend headless -W 1280 -H 720 -w 1280 -h 720
  -r 60`. The game is Gamescope's child with Gamescope WSI on, as openSU's child environment has
  it. Hardware: RX 6700 XT, RADV, Mesa 26.2.5.
- The game: `vkcube --c N` (vulkan-tools 1.4.341). Its spin advances per frame, not per time, so a
  generated frame shows as a half step.
- Why not `opensu-retro`: with `--hidden` the runner draws into an offscreen image and never
  creates a swapchain (`retro/presenter`). The layer works only in `vkCreateSwapchainKHR` and
  `vkQueuePresentKHR`, so it has nothing to apply to. The runner was therefore not used.
- The player's desktop-wide lsfg-vk v1 layer
  (`~/.local/share/vulkan/implicit_layer.d/VkLayer_LS_frame_generation.json`, no
  `enable_environment`) loads into every Vulkan process, openSU's games included. Its profile only
  matches `decky-lsfg-vk`. The runs kept it out with `DISABLE_LSFG=1`, and delivery has to do the
  same for any game that gets the fork's layer.

**How it was measured**
- Presented rate: `WAYLAND_DEBUG=server` on Gamescope logs every request it receives:
  `wl_surface.commit`, `gamescope_swapchain.set_present_mode` and its `wl_buffer.release` events.
  `lifetimes.py` turns the log into per-commit buffer lifetimes.
- Real rate: vkcube's fixed frame count over its wall time.
- Frames: Gamescope's PipeWire stream, read with `gst-launch-1.0 pipewiresrc`. That gives
  consecutive composited frames, one per vblank.
- Latch order: a scratch build of the same Gamescope commit with three stderr prints
  (`gamescope-debug-instrumentation.diff`, built in `build/lsfg-gs-debug`), read with `shown.py`.

| Run | Real frames | Commits at Gamescope (steady) | Shown |
|-----|-------------|-------------------------------|-------|
| vkcube alone, WSI on (`base`) | 59.8/s | 60.0/s | every commit |
| Layer 2x, WSI on, openSU's default (`fg2x`) | 60/s, one pair per vblank | 120/s | 60/s, about 3 in 4 generated frames dropped |
| Layer 2x, `DISABLE_GAMESCOPE_WSI=1` (`fg2x-nowsi`) | 29.9/s | 60.0/s (Xwayland's surface) | every commit, generated and real alternating |

**Answers**
- **Layer order.** App, then `VK_LAYER_LSFGVK_frame_generation`, then
  `VK_LAYER_FROG_gamescope_wsi_x86_64`, then `VK_LAYER_MESA_device_select`, then RADV. A layer
  added through `VK_ADD_IMPLICIT_LAYER_PATH` lands above the system's implicit layers.
  - Every present the layer makes, generated and real, goes through Gamescope WSI: 1200
    `set_present_mode` calls for 600 game frames.
  - The backend's own instance (created under `DISABLE_LSFGVK=1`) also gets Gamescope WSI, but it
    has no swapchain.
- **Present mode.**
  - The layer forces FIFO at swapchain creation, which also raises `minImageCount` from 3 to 5,
    and again on every present.
  - Gamescope WSI then sends MAILBOX to the driver and `set_present_mode(2)` (FIFO) to Gamescope
    before each commit, so Gamescope flags every commit FIFO.
- **Generated: yes.** The backend runs on the player's `Lossless.dll`, and the layer makes two
  presents per game present.
- **Presented under WSI: no.** `steamcompmgr.cpp:handle_done_commits_xwayland` latches FIFO
  commits one per vblank, but in the order they complete. `commit_t::Signal` appends to
  `listCommitsDone` on completion. `handle_done_commit` then erases every older commit in the
  window's queue.
  - The real frame's buffer completes before the generated one, which is presented ahead of it
    but still waits on the backend's compute.
  - Gamescope therefore latches the real frame and throws the generated one away unseen.
  - Instrumented run: 238 of about 300 generated commits were dropped this way, and no real ones.
  - Plain run: 413 of 545 generated buffers were released 33 ms after their commit, so they were
    never scanned out. Every real buffer lived about 49.5 ms. All 12 captured frames are sharp
    real frames, one cube step apart (`wsi-leftface-004-007.png`).
  - Nothing throttles the game: it runs at 60 real frames per second, and the GPU generates
    frames nobody sees.
- **Presented without WSI: yes.** This is lsfg-vk's documented workaround.
  - Xwayland's Present queue keeps commit order, and FIFO throttles the game to 30.
  - Gamescope shows 60 frames per second, and consecutive captures alternate soft generated and
    sharp real frames, a half cube step apart (`nowsi-leftface-004-007.png`: 004 and 006 are
    generated).
- **Validation.** `VK_LAYER_KHRONOS_validation` from `VK_INSTANCE_LAYERS` lands next to the
  driver, below the layer. So it checks every call the layer makes on the game's device and the
  whole backend device, but not the game's own calls.
  - With WSI off there were 0 errors.
  - With WSI on, two errors also show with vkcube alone, so they are Gamescope WSI's: it enables
    `VK_EXT_swapchain_maintenance1` without `VK_EXT_surface_maintenance1`, and every present
    carries a `VkSwapchainPresentModeInfoKHR` with `swapchainCount` 0.
  - Also with WSI on, `VK_KHR_wayland_surface` and `VK_KHR_xcb_surface` are enabled without
    `VK_KHR_surface` in the backend's instance, where Gamescope WSI adds them.
  - None of the errors come from the layer.
- **Not checked.** The nested (Wayland backend) Gamescope: the latch code is the same and only
  the vblank source differs. Also unchecked: present-wait and present timing through the WSI, a
  game openSU's shell starts itself, and Steam or pressure-vessel.

**For the layer-or-Gamescope decision**
- The layer route needs a change in our Gamescope fork before any generated frame shows with WSI
  on.
  - The change: a FIFO commit must not be latched while an older commit of the same window is
    still pending. It sits in `handle_done_commits_xwayland` and `handle_done_commit`. It looks
    small but has not been tried.
  - Turning WSI off for frame-gen games works today. It gives up what openSU keeps WSI for, and
    real frames go through Xwayland.
- Even with that fix, both images of a pair arrive back to back from one `vkQueuePresentKHR`, so
  Gamescope paces them only by FIFO vblank. That suits fixed integer multipliers. Adaptive output
  still needs the layer's own present thread (step 3 of "What adaptive takes").
- The compositor-side alternative avoids this queue, because Gamescope would hold both real
  frames itself. Step 1 does not settle the choice. It adds one Gamescope fix to the layer route's
  cost and shows that route working today only with WSI off.

## Lossless.dll for openSU

Never copied. openSU resolves the path inside the player's install and passes it on. It writes
`dll = '<path>'` in the config it generates, or `LSFGVK_DLL_PATH` (honoured only with
`LSFGVK_ENV=1`).
- The resolver walks every Steam library that Settings › Steam libraries already reads
  (`libraryfolders.vdf`, `app/steam_library_settings`) for
  `steamapps/common/Lossless Scaling/Lossless.dll`. That covers libraries such as
  `/mnt/Boy/SteamLibrary`, which the fork's resolver misses.
- No install gives a refusal naming Lossless Scaling on Steam (app 993090).
- Path visibility: the Steam Runtime container may hide other partitions (upstream's `dll`
  option note). Isolated Steam's bwrap must bind the folder.

## Per game in openSU

Written for the layer route. Under "Decision" only the layered settings below still hold; the
layer, `conf.toml` and per-process environment are not needed.

- **The layer.** Ship the fork's layer beside the runner with its own manifest in openSU's data
  dir. Load it only in processes openSU starts, through `VK_ADD_IMPLICIT_LAYER_PATH` or an
  `enable_environment` key in the fork's manifest, never the player's desktop-wide
  `implicit_layer.d`. Without a matching profile the layer does nothing.
- **Config.** openSU writes one generated `conf.toml` (in its runtime or config dir) and points
  `LSFGVK_CONFIG` at it. The file holds a profile per game with frame generation on. A change
  from the game settings page rewrites it, and the layer reloads it at the next present.
- **Steam games** (Managed or Isolated Steam, openSU's own client):
  - Games inherit the Steam client's environment, so `LSFGVK_CONFIG` and the layer path are set
    once on the client.
  - Profiles match by `active_in`: the Linux binary, or the Proton `.exe` (the fork already
    reads it from the cmdline). Matching by App ID (`SteamAppId` in the game's environment) is a
    small addition to the fork's `detection.cpp`.
  - Not verified: whether pressure-vessel imports a layer from `VK_ADD_IMPLICIT_LAYER_PATH`. It
    does import the host's standard layer dirs. This needs a check in the product.
- **Wine/Proton** (GOG, Epic via `library/wine`) and **libretro** (`opensu-retro`, raw Vulkan):
  openSU starts the process, so the per-launch environment carries `LSFGVK_CONFIG` and
  `LSFGVK_PROFILE=<game id>`.
  - Runner caveat: a core that draws at 20 or 30 but presents every 60 Hz frame gives the layer
    repeated frames, so it gains nothing.
  - Runner caveat: the runner's own frame pacing to the core rate must stay off the game's
    present path once the layer paces.
- **Layered settings.** This fits `settings/setting_layers` as-is: `framegen.mode`
  (off/fixed/adaptive), `framegen.target` (a number or `display`), `framegen.max_generated`,
  `framegen.tolerance`, `framegen.flow_scale`, `framegen.performance` at Global, System,
  Emulator and Game.
  - A store game has only Global and Game, because its system and emulator keys are empty.
  - openSU resolves `display` to the monitor's refresh, which `app/shell_window` already reads.
  - A new `settings/framegen_settings`, beside `video_settings`, owns the keys and the profile
    writer.

## Build order

The layer route's steps (the fork layer, its backend timestamps, its present thread) are
superseded by "Decision". Step 1's finding stands as the reason.

1. Mechanism in the Gamescope fork with the linear-blend stand-in: done ("Proof").
2. Phase B, adaptive pacing in `FramePacer`: done ("Proof").
3. Our MIT generator behind `IFrameSynthesizer`, clean-room from the `Lossless.dll` behaviour spec.
4. Phase C, openSU: the `framegen.*` setting keys on the layers, the atoms set for the focused
   game, rows on the game settings tabs and Settings › Emulators.
5. Record: the codemap rows, project-state rows, this card closed.

Web pages read for this card gave no instructions aimed at an agent. The upstream README only
points to the new site.
