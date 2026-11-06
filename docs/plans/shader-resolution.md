# Shader resolution

Requested 2026-11-05: Kirby lags on the Huawei tablet (Kirin T90A, 2800x1840) because heavy CRT
presets such as crt-1tap-bloom are too much for its GPU at the screen's full resolution
(`docs/design/libretro.md`, Pacing: 33/s, fence 16-20 ms). A "Shader resolution" setting lets the
runner draw a preset's viewport-sized passes at a lower resolution only while the GPU cannot keep
up.

## What it does

`video.shader-resolution`, shown under a shader preset, at every setting layer:

- **Auto** (default): the preset's chain is drawn at the full resolution of the rectangle the frame
  is placed in until the GPU's time for a frame passes the budget; it then steps down, and back up
  once there is headroom again. A PC that keeps up never steps down.
- **Full**: always the full resolution.

The runner logs each step: `shader resolution down to 70% (GPU 19.4 ms of a 16.7 ms frame)` and
`shader resolution back up to 85% (GPU 6.1 ms of a 16.7 ms frame)`.

## librashader

The pinned fork's C API has no scale override: `libra_viewport_t` is an origin and a size inside
the output image, and the Vulkan runtime sizes every `viewport`-scaled pass from the output image's
own size (`filter_chain.rs`, `scale_output_framebuffers_with_context(.., viewport.output.size, ..)`),
the last pass drawing into the viewport. So the lower resolution comes from drawing the whole chain
into a smaller image of the placed rectangle's shape and drawing that image bilinearly into the
rectangle. Every viewport-scaled pass, every source-scaled pass after one, and the last pass shrink
with it; source-scaled passes before the first viewport pass and absolute ones do not change. No
fork change is needed.

## Owners

- `src/settings/video_settings.*`: the setting (`ShaderResolution`: Auto, Full), its choices,
  its declaration shown only under a preset, resolution, and its JSON in the launch file and
  `POST /video`.
- `src/retro/shader_scale.*` (new, `ShaderScale`): the scale governor. It takes each frame's GPU time and
  the frame's budget (the core's frame interval) and holds the chain's scale among 100, 85, 70, 60
  and 50 %. It steps down one level when the median of a 30-frame window is over 90 % of the budget,
  and up one level after four windows in a row whose median, scaled by the area the step up adds,
  stays under 75 % of the budget. Full holds 100 %. It logs each step.
- `src/retro/gpu_timer.*` (new): two Vulkan timestamps around the chain's commands, read after the
  frame's fence. A queue without timestamps leaves Auto at full resolution and says so once.
- `src/retro/reduced_output.*` (new): the image the chain draws into below full resolution, made
  again when its size changes, and its bilinear draw into the placed rectangle through the
  compositor's render pass.
- `src/retro/frame_compositor.*`: composes them in `recordChain`. `FrameImage` gains the frame's
  `interval`, which the runner fills from the core's timing.
- `src/retro/shaders/sampled.frag`: the overlay's shader renamed, as the reduced output draws with
  it too (the sampled image as it is).

## How to check it

- Linux: `opensu-retro --hidden` with crt-1tap-bloom under Auto logs no step; with the target made
  large (`OPENSU_WIDTH=16384 OPENSU_HEIGHT=4096`) Full falls to 31/s while Auto logs the steps
  down, holds the core's rate, and steps back up when `POST /video` lowers the work.
- The tablet: crt-1tap-bloom under Full holds the old 33/s; under Auto it logs the steps down to
  50% and holds 49/s, short of the core's rate
  (`docs/issues/retro-frame-waits-for-the-gpu.md`).

## State

Built and shown on Linux 2026-11-05 (`docs/design/libretro.md`, Shader resolution). Shown on the
tablet 2026-11-06 with 0.6.0: Full 33/s with the shader at 24 ms, Auto 49/s with it at 13 ms.
