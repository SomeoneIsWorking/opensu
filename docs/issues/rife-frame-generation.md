# RIFE as a frame generator is too slow for low-end hardware

Status: closed, not pursued. Decided 2026-11-15: frame generation stays on Lossless (and `flow`
where Lossless cannot run); RIFE was removed.

The player asked for machine-learning frame generation; the bar they set: "if it's not runtime
viable for low-end hardware then it is not viable", for high resolutions too, with a flow scale
of 75% at the least for PC games at 1080p.

## What was built and measured

RIFE 4.x (Practical-RIFE, MIT, in TNTwise's ncnn form) ran on openSU's own generator device as
compute shaders of ours, one dispatch or buffer copy per layer of the graph, matched blob by blob
to a CPU reference. The code is in the history: `95aabc8b`, `286cf4e5` and `b2518af6`, reverted
together.

Multiply-adds per generated frame, from the graph:

| Frame the network runs on | v4.26 | v4.25-lite |
|---|---|---|
| 240 x 160 (GBA) | 0.86 G | 0.42 G |
| 256 x 224 (SNES) | 1.28 G | 0.62 G |
| 1440 x 816 (1080p at 75% flow scale) | 26 G | 12.6 G |
| 1920 x 1088 | 46 G | 22 G |

GPU time per generated frame with those first shaders (`opensu-framegen-pairs --measure time`):

| Device | 240 x 160 | 256 x 224 | 1280 x 720 |
|---|---|---|---|
| RX 6700 XT | | 19 ms | |
| ROG Ally (Z1 Extreme), v4.25-lite | 24 ms | 42 ms | 305 ms |
| ROG Ally (Z1 Extreme), v4.26 | 24 ms | 40 ms | 516 ms |

The intermediate blobs held without reuse took 2.5 GB at 720p and 5.4 GB at 1080p.

## Why it stops

Faster shaders (tiled convolutions, FP16, fused layers, blob reuse) would remove most of the
measured time, but not the arithmetic. At 1080p with 75% flow scale the lite model needs 12.6 G
multiply-adds per frame; on the Ally's GPU that is about 4 to 6 ms at the efficiency the best
convolution code reaches, against an 8 ms budget at 120 Hz that the game itself is already
using. The tablet (Kirin T90A) is further off. A network small enough would have to be trained
for real-time play, a project of its own.
