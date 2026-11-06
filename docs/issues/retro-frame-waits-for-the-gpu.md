# The runner's frame waits for the GPU

Observed on the Huawei tablet (Kirin T90A, 2800x1840) with 0.6.0 and crt-1tap-bloom: Auto settles at
50% and 49/s where the core wants 59.7/s. The shader takes 12.7 ms, inside the 16.7 ms budget, but
each frame runs core 4 ms, record 3.8 ms, fence 10.6 ms and present 1.1 ms one after another, 20 ms
in all. Auto has no lower step, and a lower one would not reach the budget by much.

Likely fix: let the core run the next frame while the GPU draws this one, so the frame costs the
longer of the two rather than their sum. Not yet traced in the runner's loop.

Check: `frames in` lines in logcat with a shader preset on the tablet.
