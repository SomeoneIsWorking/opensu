# Real vblank times for the remote guest's frame pacing

Asked for 2026-11-13, from the frame generation pacing work (`remote-frame-generation.md`).

## Target

`frame_gen::PacedFrames::vblank` gets the time the display showed the shell's last frame, or the
next vblank's, on every platform the remote guest runs on, instead of the clock when the shell
loop asks for a picture. `framegen::CFramePacer` takes its vblank grid from the latest time it is
given (fork `framegen/include/framegen/FramePacer.h`), so a time seen late moves the grid: in
`p-pin` one present seen 0.3 ms late left every later in-between 0.25 ms (t 0.507) off the half
step for the rest of the run, and in `remote-skipped-capture` (fork `framegen/tests/data/`)
presents up to 10 ms late make any snap of the claimed vblanks onto that grid show content a
refresh off at 48 vblanks, against 13 without the snap. Gamescope gives the pacer real vblank
times and shows neither. Proven when a remote run's in-betweens sit within 0.005 of t 0.5 outside a step's re-align, a
late shell pass (a stall in the loop) included.

## What the shell has today (Linux)

- gpu2d presents through SDL GPU on Vulkan, `SDL_GPU_PRESENTMODE_VSYNC` (FIFO) when shown and
  `IMMEDIATE` or `MAILBOX` when hidden (`gpu2d/src/render/gpu_window.cpp`); `ShellWindow::pace`
  then sleeps to the next present on a steady-clock timer (`presentInterval()`: the display's
  refresh while a remote game generates, 60 otherwise).
- The time the pacer gets is `stream::wire::nowMicros()` in `ShellApp` before
  `RemotePlay::takePicture`, so it carries the timer's wake and the loop's work before it.
- SDL GPU has no present timing in its API, and the SDL fork's Vulkan backend
  (`src/gpu/vulkan/SDL_gpu_vulkan.c`) enables neither `VK_KHR_present_id`/`VK_KHR_present_wait`
  nor `VK_GOOGLE_display_timing`, and calls `vkQueuePresentKHR` itself. Its Wayland backend binds no
  `wp_presentation` (presentation-time).

## Options

- Linux, Vulkan: `VK_KHR_present_id` and `VK_KHR_present_wait` in the SDL fork's Vulkan backend: an
  id on each present and an SDL call that waits for the id to be shown, with the time it returns
  taken as the vblank; Mesa's X11 and Wayland WSI and NVIDIA's driver to be checked for it on the
  guest's versions. A wait gives the moment it returns, not the display's timestamp, so it is late
  by a wake but never by the shell loop.
- Linux, `VK_GOOGLE_display_timing`: the display's own present times (`actualPresentTime`), also in
  the SDL fork's backend; which drivers offer it on X11 and Wayland to be checked.
- Linux, Wayland: `wp_presentation` feedback on the SDL window's `wl_surface`
  (`SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER`), requested before each present on its own event
  queue, gives the compositor's presentation time and refresh. Outside Vulkan, so it works whatever
  the driver, but only on Wayland, and the feedback must land on the commit the WSI makes.
- Windows: the guest draws through SDL GPU on Vulkan too; `VK_KHR_present_wait` is in AMD's and
  NVIDIA's Windows drivers (to be checked per vendor); DXGI frame statistics are out of reach
  without a D3D swapchain.
- Android: `VK_GOOGLE_display_timing` (Android's own extension) or `AChoreographer`'s vsync
  callback, which gives the vsync time and the next ones.
- Every platform without one of these keeps today's time.

## Order

1. The SDL fork: present id and present wait in the Vulkan backend, one SDL call for the shown time
   of the last present; gpu2d's `GpuWindow` reports it.
2. `ShellWindow` passes it on with each present; `RemotePlay::takePicture` gives `PacedFrames` that
   time instead of the loop's clock.
3. Proof: a remote run with a stalled loop pass logged, and its replay through the fork's
   `framegen_pacer_replay`.
4. Android through `VK_GOOGLE_display_timing` or `AChoreographer`; Windows as Linux.
