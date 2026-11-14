# RIFE frame generation

Asked for 2026-11-14: a learned generator beside `flow`, because `flow` breaks where block motion
cannot decide what is in front, the ghosted edges around a moving sprite (Kirby's title) and
repeating textures that match at several offsets (Super Metroid's grass, the near-tie case in
`remote-frame-generation.md`, "Quality"). The streaming side-by-side videos are recorded once this
generator runs.

## Target

- A third choice for `framegen.generator`, RIFE, behind the one `frame_gen::Interpolator` interface,
  so the runner (`retro/frame_generation`), the remote guest (`app/remote_frame_gen`) and
  `opensu-framegen-pairs` take it without their own paths.
- Better than `flow` on the quality harness's scenes (`tools/framegen_quality`), judged on the
  moving frames and by eye on the worst ones, at a cost the Ally holds at 60 to 120 on a core's
  own frames.

## The model

RIFE 4.x (hzwer's Practical-RIFE, MIT), in the ncnn form of TNTwise's `rife-ncnn-vulkan` fork
(MIT): `flownet.param` (text graph) and `flownet.bin` (weights, about 11 MB). First
`rife-v4.25-lite`, then `rife-v4.26` against it. The graph uses 13 layer types: Convolution,
Deconvolution, PixelShuffle, Interp, the `rife.Warp` custom layer, BinaryOp, ReLU (leaky),
MemoryData (constants such as PReLU slopes), Split, Crop, Concat, Eltwise, Sigmoid.

The model reaches the player like a core: a pinned file with its checksum, downloaded on first use
into openSU's data directory from an openSU release asset (the MIT files re-hosted with their
notice), never in git and never in a package.

## Where it runs

On the generator device openSU already has (`frame_gen::GeneratorDevice`), as compute shaders of
our own, not through ncnn:

- One device: the frames are already `GeneratorImage`s there; ncnn would open a second Vulkan
  device and every frame would cross between them.
- One measurement: `GpuSpans` counts its work like `flow`'s, so the `perf-units` comparison holds.
- One build: no ncnn and glslang to build per platform and per Android ABI.
- The cost: the layer shaders are ours to write and check. The op set is small and fixed by the
  model, and nothing beyond it is implemented.

Pieces, each in `src/frame_gen/`:

- `ncnn_model`: parses `flownet.param` and `flownet.bin` into a graph (layers, blobs, weights);
  refuses a layer type or parameter it does not implement, naming it.
- `rife_net`: the graph on the device, one compute pipeline per layer type, blobs in one fp32
  storage buffer, recorded once per input size.
- `rife_interpolator`: the `Interpolator`, taking each real frame at its `SourcePixel` size (a
  core's own 256 x 224, not the scaled-up screen), padding to the model's multiple (64 for v4.26, 128 for v4.25-lite, from the graph's strides), and
  writing the frame between at t into the output image.

## Proof

1. `ncnn_model` reads both models; a test feeds the graph a tiny input on the CPU reference (each
   op written plainly, in the test) and on the device, and compares every blob.
2. `opensu-framegen-pairs --generator rife` on the nine harness scenes: scores against `flow`
   and the `repeat`/`blend` references, and the worst frames looked at side by side.
3. The Kirby title and Super Metroid's grass by eye at 30 to 60 and 60 to 120.
4. Cost in `GpuSpans` units on this PC, the Ally and the tablet.
5. In the product: the runner on a core's game, then the remote guest; the side-by-side videos
   recorded then (`scratch/ally-guest`, guest with a mapped window inside headless Gamescope, as
   a hidden window is never captured).

## Status

- Step 1, the model read: `ncnn_model` reads both models; `tests/frame_gen/rife_reference` runs
  the graph on the CPU (about 1 s a frame at 256 x 224). Its frame between Kirby's title frames 26
  and 28, and Super Metroid's demo frames 10 and 12, at their native size: coherent, Samus whole
  mid-stride where `flow` breaks her apart. The models are pinned at TNTwise/rife-ncnn-vulkan
  13338e38 under `env-assets/rife/` (sha256 `flownet.bin`: v4.25-lite 350a15e4…,
  v4.26 94d58e30…).
- Step 1, on the device: `rife_net` and `rife` (`opensu-framegen-pairs --generator rife`). Every
  blob of both models agrees with the reference from noise inputs within 1e-2 of its largest
  value (`rife_net_check`, at the multiple and wider); on Kirby's frames 26 to 28 and Super
  Metroid's 10 to 12 the device's frame differs from the reference's in 2 and 8 values, by one
  level each.
- Next: step 2, the harness scenes against `flow`.
