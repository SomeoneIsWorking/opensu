# Two cores die in the runner

Status: open for Final Fantasy Adventure. The cause is in the core and the proper fix is still to be
decided. Crash Bandicoot is fixed; its section stays below as the record.

Found on 2026-11-04 while proving save transfer. Both happen without a save, and on the installed
`opensu-retro` as well as a fresh build.

## Final Fantasy Adventure on the mGBA core dumps core

Observed: `opensu-retro` with the buildbot's `mgba_libretro` (`mGBA 0.11-212-7a12d6d`) and
`Final Fantasy Adventure (USA).gb` (MBC2) shows its first frame, then dies of SIGSEGV in
`retro_run`. Expected: the game runs, as Kirby & the Amazing Mirror does on the same core.

Reproduce: `opensu-retro --hidden --frames 300 --core <cores>/mgba_libretro.so --saves <empty>
--system <empty> "Final Fantasy Adventure (USA).gb"` under gdb, with an empty saves folder.

Cause (the core's): the buildbot builds libretro/mgba, whose head 7a12d6d split from upstream before
upstream's 543a197, "GB MBC: GBMBCSwitchSramBank should always assign a valid sramBank pointer
(fixes #3859)". Upstream issue #3859 is this crash: "MBC2 games segfault on the first read of
cartridge RAM". On the first `retro_run`, libretro.c's `_doDeferredSetup` calls `loadSave`.
`GBLoadSave` frees the SRAM it had (`munmap` of that page), maps the frontend's buffer and calls
`GBMBCSwitchSramBank`. For SRAM under 8 KiB (MBC2 has 0x100 bytes), 7a12d6d returns from
`GBMBCSwitchSramBank` without repointing `sramBank`. The game's first cartridge RAM read in
`_GBMBC2Read` then goes through the freed page. In gdb, the catchpoint on `munmap` of
`0x7ffff7f8d000` is hit from `retro_run`, and the next SIGSEGV is
`movzbl (%rdx,%rax,1)` with `rdx = 0x7ffff7f8d000` (`sramBank`). The runner's save RAM span
(`0x7ffff7f8e000`, 256 bytes) is the new buffer and the runner never writes it before the crash.

What the player sees: the shell names a death by signal (`game_end`'s `crashName`), so the launch
ends with "Final Fantasy Adventure crashed (SIGSEGV)". It is not a silent freeze. A segfault inside
the core cannot be caught in the runner's process; the shell's notice is the containment.

Proper fix (not done; whether to ask libretro/mgba is the user's call): a core that carries
543a197. Either libretro/mgba syncs upstream, or openSU builds `mgba_libretro` from a maintained
fork pinned to a commit that has it, served through the catalog's `"release"` the way other own
builds are. Until then, Gambatte (built-in) runs Game Boy games.

## Crash Bandicoot on PCSX ReARMed loses the GPU device (fixed)

Observed: `pcsx_rearmed_libretro` (r26l-137-g7b0e8f8, lightrec) on Crash Bandicoot ran 27 frames.
Then RADV reported `GPUVM fault detected` (client TCP, a read) and
`The CS has been cancelled because the context is lost`, and the runner logged
`vkQueueSubmit failed: VkResult -4`. With `--validate` it never faulted.

Cause: `src/retro/frame_compositor.cpp:FrameCompositor::bindTexture` rewrote the descriptor only
when `SampledInput::bound()` differed from the texture's view. At frame 27 the core's software frame
goes from 640x480 to 512x240, and `StreamedTexture::ensure` destroys the texture and makes it again.
RADV handed back the same `VkImageView` pointer, so the descriptor still held the freed image and
the draw read unmapped memory. In each of 3 faulting gdb runs the new view equalled the old one; in
the one run that went on, it differed. The validation layer wraps handles uniquely, which is why
validation hid it.

Fix: the compositor binds the texture again whenever `ensure` reports a remake, as `OverlayPass`
already did, and `SampledInput` no longer keeps the bound handle. Proof (`scratch/runner-crashes/`
of that session): in 5 gdb runs the handle was reused at the resize and the texture was bound again,
and all 5 ran on. 10 of 10 plain 300-frame runs ended cleanly, and a 1500-frame run shows the
Naughty Dog logo (`after-long/last.png`).
