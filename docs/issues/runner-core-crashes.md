# Two cores die in the runner

Status: open. Cause unknown. Found on 2026-11-04 while proving save transfer; both happen without
a save, and on the installed `opensu-retro` as well as a fresh build.

## Final Fantasy Adventure on the mGBA core dumps core

Observed: `opensu-retro` with `mgba_libretro` and `Final Fantasy Adventure (USA).gb` (MBC2) logs
the core, `160x144 at 59.728 fps`, `drawing on AMD Radeon RX 6700 XT (RADV NAVI22)`, then the
process dumps core within the first frames. Expected: the game runs, as Kirby & the Amazing Mirror
does on the same core.

Reproduce: run the runner hidden on that ROM with an empty saves folder (logs in
`scratch/saves3/ffa/nosave.log` and `withsave.log` of that session). Next: a backtrace from the
core dump (`coredumpctl gdb`) to tell the core's fault from the runner's.

## Crash Bandicoot on PCSX ReARMed loses the GPU device

Observed: `pcsx_rearmed_libretro` (r26l-137-g7b0e8f8, lightrec) on Crash Bandicoot runs about 27
frames, then RADV reports `GPUVM fault detected`, `The CS has been cancelled because the context is
lost` and the runner logs `vkQueueSubmit failed: VkResult -4` (device lost). SwanStation runs the
same disc. Untested leads: the runner's upload of the core's software frames, and the core's fixed
mappings (`Regular mmap to address 0x2fc00000`).

Reproduce: run the runner hidden on the disc with the core (logs in `scratch/saves3/psx-direct/`
of that session). Next: run with `VK_LAYER_KHRONOS_validation` and compare the frame upload path
against SwanStation's.
