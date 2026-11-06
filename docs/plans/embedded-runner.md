# Embedded runner

Requested 2026-11-06: a game on a built-in core draws inside openSU's own window, with the core
still in a process of its own, so starting one is no visible switch and a core crash leaves the
shell up with its toast. Android first, behind Settings › Emulators "Embedded", off until it is
proven on the tablet. `docs/design/libretro.md` §2 is the arrangement this replaces in that mode.
Built behind the setting; not yet run on a device.

## Android

The shell keeps one Activity. A launch with `--embedded` on its command line (`prepareRetroLaunch`,
from the one setting) adds a `SurfaceView` under SDL's in `ShellActivity`, binds `RetroService` in
`:retro` and passes the view's `Surface` over Binder. `Surface` is Parcelable and its producer works
from another process (Chromium's GPU process, `IGpuProcessCallback.getViewSurface`, runs on this);
the runner makes its `ANativeWindow`, `VkSurfaceKHR` and swapchain from it and presents straight to
it, no frame copied. `SurfaceControlViewHost` would need API 30; the floor is 28.

| Piece | What changes |
| --- | --- |
| Presenter | The window leaves it for a seam, `RunnerWindow`: `SdlRunnerWindow` as today, `HostedRunnerWindow` on the passed `Surface`. Instance, device, queue, compositor, overlay and `dropSurface` do not change |
| SDL | No fork change. Android's SDL is upstream 3.4.14 pinned by android-port, not the fork. The video and input halves need an `SDLActivity`, so `:retro` opens neither; the rest takes a `Context` declared as `Activity`, which `RunnerContext` satisfies. `nativeRunMain` and AAudio need no Activity (`ALLOW_MULTIPLE_ANDROID_AUDIO_DEVICES` is 0) |
| Sound | SDL's AAudio in `:retro`, unchanged |
| Input | No Activity in `:retro` means no key, pad or touch events there. The shell hears them all: `RunnerRelay` watches SDL's events as they are pushed and posts them, off the loop, to the channel's `/input`, `/touch` and new `/host`, the routes hidden runs use. Guide and Back stay in the shell, as on Linux |
| Touch panel | Still the runner's, drawn into its own swapchain by `OverlayPass`; `/host` gives it the window's dp and safe area and tells it when it is covered |
| GPU cores | Unchanged: they see the runner's instance, device and queue, and the extensions are the two SDL named. A lost surface loses the swapchain only |
| Lifecycle | `surfaceDestroyed` sends a null surface; the game waits, the device stays, and the next `Surface` makes a swapchain. A resize makes one too. Every message is oneway, so a hung runner cannot stall the shell. A shell that dies takes `:retro` with it (`onDestroy`) |
| Launch | `Launches` keeps one start, stop and exit record; `RunnerHost` (`TaskRunnerHost`, `EmbeddedRunner`) is the only difference. Zip staging and `--beside` move from `RetroActivity` to `GameStaging`, which both use |

What goes away in this mode: `seeThrough`, `overGame`, task bouncing, `resume`'s `startActivity`,
`openShellMenu`'s `startActivity`. They stay for the default mode and go with `RetroActivity` once
Embedded is the default.

**Launch feel.** The card shows the game's cover and "Starting" and stays until the runner's
`/status` says a frame reached the surface (`Handoff::Hooks::shown`); the shell turns clear only
then, so the handover is a cut. Staged files live in `staged/<game's folder>/`, kept while the
document's size and date hold, the oldest removed past 4 GiB.

## Risks

- Scheduling: a bound service may be placed below an Activity. `BIND_IMPORTANT` asks for the
  client's group; `frame_stages` on the tablet shows whether it holds.
- The shell keeps presenting clear frames over the game: a second full-screen layer and a GPU
  client. If it costs frames, the shell stops presenting while nothing of it shows.
- Input takes an HTTP post per event (loopback, no frame-loop wait) instead of a local queue.
- On API 31+ frames pass through the shell's `BLASTBufferQueue` (a buffer handle, no copy): a shell
  stalled on its binder threads stalls the game. Never its main thread.
- `RunnerContext` is an `Activity` that never attaches: anything in SDL touching its window throws.
  The proper fix is SDL's context typed `Context`, in a fork that android-port pins; that rebuilds
  the machine's shared Android prefix for every port, so it waits on a reason beyond this.
- A crash drops the game's view as soon as the binding dies; the shell stays clear until
  `Launches.running` notices, up to the 750 ms poll plus the exit record's wait.

## Desktop, not built

| OS | Route | Cost |
| --- | --- | --- |
| Linux | Runner exports dma-buf images (`VK_EXT_external_memory_dma_buf`, DRM modifiers) over a socket with their sync files; the shell imports them through the SDL fork's DMA-BUF texture, as `RemotePicture` does, and draws them in its own window inside its Gamescope | A swapchain of our own, one more GPU pass and up to a shell frame of latency, in return for nothing the player sees: Gamescope already shows one screen. Last in line |
| Windows | Shared texture: `VK_KHR_external_memory_win32` NT handles and a timeline semaphore, sampled by the shell, which ends the last-frame Guide backdrop and the DirectComposition question. Child HWND is cheaper to start and worse to finish: panels cannot draw over it without DirectComposition, and input queues attach across processes | The SDL fork's D3D12 GPU backend needs an import property. One GPU pass and a frame of latency. First in line |
| macOS | `IOSurface` per image, the shell wrapping each as a Metal texture; `VK_EXT_metal_objects` on the runner's MoltenVK, mach port over XPC or a socket | Depends on MoltenVK's export; the overlay already shows the game live, so it only removes the window switch |
