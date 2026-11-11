# The runner's frame waits for the GPU (fixed on the PC; tablet not yet checked)

Observed on the Huawei tablet (Kirin T90A, 2800x1840) with 0.6.0 and crt-1tap-bloom: Auto settles at
50% and 49/s where the core wants 59.7/s. The shader takes 12.7 ms, inside the 16.7 ms budget, but
each frame runs core 4 ms, record 3.8 ms, fence 10.6 ms and present 1.1 ms one after another, 20 ms
in all. Auto has no lower step, and a lower one would not reach the budget by much.

Reproduced on the PC (RX 6700 XT): `opensu-retro --hidden --validate` on mGBA with Kirby & the
Amazing Mirror, crt-1tap-bloom stretched at Full over a 16384x2200 target (`OPENSU_WIDTH`,
`OPENSU_HEIGHT`). The shader takes 16.3 ms of the GPU, under the 16.7 ms interval, and the runner
held 56.0/s: frame 17.7 ms, core 0.5, fence 15.9, record 1.3, sleep 0.

Cause: `src/retro/presenter.cpp:Presenter::present` had one command buffer and one fence, and
waited for the frame before to be done before recording the next, so the GPU sat idle while the
CPU ran the rest of the core's frame and recorded the next one: a frame cost the GPU's time plus
the CPU's part after the fence, not the longer of them.

Fix: software cores keep two frames in flight. Each frame slot has its command buffer, fence,
acquire semaphore and, hidden, its offscreen image; the presenter waits only for the frame that
last used the slot, two back. What a frame writes from the CPU is per slot too: `StreamedTexture`'s
staging buffer, `GpuTimer`'s timestamp pair and the compositor's descriptor set; librashader's chain
is built with `frames_in_flight` 2. GPU cores stay on one (design doc, GPU cores). The added
latency is at most one frame interval, and only while the GPU cannot keep up.

Proof on the PC, same command, before and after (`scratch/frames-in-flight/runs/` of that session,
`before-*` and `after-*`, each `run.log` and `last.png`):

| Target | Before | After |
| --- | --- | --- |
| 16384x2200 (shader 16.3 ms) | 56.0/s, frame 17.7, fence 15.9, record 1.3 | 59.7/s, frame 16.7, fence 0.0, sleep 14.8 |
| 16384x2000 (shader 14.8 ms) | 59.7/s, fence 0.0 | 59.7/s, fence 0.0 |
| 16384x2400 (shader 17.7 ms, over budget) | 46.7/s falling to 10-16/s, fence 50-100 ms | 55.2/s steady, frame 17.8, fence 15.9 |

Validation was clean in every run. Crash Bandicoot on PCSX ReARMed (the device-loss game, whose
frame shrinks from 640x480 to 512x240 at frame 27 and remakes the texture) ran 1500 frames at
60.0/s with validation, plain and with crt-1tap-bloom, to the Naughty Dog logo, and three 600-frame
runs without validation ended cleanly. Mupen64Plus-Next (ParaLLEl-RDP and RSP) on Ocarina of Time,
one frame slot, reached the title screen at frame 1500 with validation clean.

Left: the tablet check with crt-1tap-bloom (`frames in` lines in logcat), which is the player's.
