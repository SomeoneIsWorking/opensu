# The overlay blacks out the game

Status: fixed, verified in a headless Gamescope with a stand-in game; the player's check on the
desktop shortcut is still open.

Observed (player, desktop shortcut, every game tried): while the Guide menu or the quick menu is
open over a game, the game is black. Expected: the game shows through the dimmed overlay.

## Cause

`session/compositor_session.cpp:CompositorSession::runOnce` started Gamescope without turning the
Gamescope WSI layer off for the shell, so the shell's own Vulkan swapchain went through it:

1. Gamescope sets `ENABLE_GAMESCOPE_WSI=1` for every child (`steamcompmgr.cpp:LaunchNestedChildren`),
   which enables the implicit layer `VK_LAYER_FROG_gamescope_wsi` (system package, enable
   variable `ENABLE_GAMESCOPE_WSI`, disable variable `DISABLE_GAMESCOPE_WSI`).
2. The layer turns the shell's X11 surface into a `wl_surface` on Gamescope's Wayland server
   (`VkLayer_FROG_gamescope_wsi.cpp:CreateGamescopeSurface`) and, for a top-level window with no
   children, presents on it (`canBypassXWayland`; log `Creating swapchain for xid ... flip: true`).
   Its `GetPhysicalDeviceSurfaceCapabilitiesKHR` reports the Wayland surface's capabilities.
   Cited from the fork's copy (`build/fork-gamescope/layer/`); the layer loaded is Fedora's
   gamescope 3.16.29 package's, and its log shows the same path.
3. Mesa 26.2.4's Wayland WSI offers `OPAQUE | PRE_MULTIPLIED` composite alpha
   (`wsi_common_wayland.c`, `supportedCompositeAlpha`), not the X11 swapchain's
   `INHERIT | PRE_MULTIPLIED` on a 32-bit visual (`wsi_common_x11.c`).
4. SDL 3.4.18 takes `OPAQUE` whenever it is offered and looks further only for a window created
   with `SDL_WINDOW_TRANSPARENT` (`SDL_gpu_vulkan.c`, swapchain creation), a flag SDL GPU refuses
   (`SDL_gpu.c:SDL_ClaimWindowForGPUDevice`).
5. Mesa picks the buffer format from `compositeAlpha == PRE_MULTIPLIED`; anything else makes
   `B8G8R8A8` an `XRGB8888` buffer (`wsi_common_wayland.c:wl_drm_format_for_vk_format`).
6. Gamescope samples an alpha-less format with alpha forced to one
   (`rendervulkan.cpp:2556-2575`, `DRMFormatHasAlpha`, `VK_COMPONENT_SWIZZLE_ONE`), so the overlay covers the
   game opaquely and its premultiplied scrim (0,0,0,115) shows as black.

The X window's depth stays 32, so `GamescopeOverlay`'s depth check passed; Gamescope never reads the
buffer Xwayland would have made from that visual.

## Fix

The shell runs without the layer and goes through Xwayland on its 32-bit visual, as the design
intended; games keep it. `CompositorSession::runOnce` gives Gamescope (and so the shell)
`DISABLE_GAMESCOPE_WSI=1` (`launch::gamescopeWsiOff`), and `launch::ChildEnvironment` leaves that
variable out of every program openSU starts (`Instance::start`, `runCommand`, `runStreaming`,
`runCaptured`, `runCapturedWithLine`), so games, Steam and the libretro runner load the layer as
before.

Not chosen: a `PRE_MULTIPLIED` swapchain through the layer needs SDL changed (a maintained, pinned
SDL fork built in place of the system library) for no gain over Xwayland for a 2D overlay.

A `DISABLE_GAMESCOPE_WSI` the player sets on the desktop no longer reaches games started by openSU.

## Evidence

Headless Gamescope (the pinned fork, `--backend headless`, 1280x800) started by the real
`CompositorSession`, with a stand-in for the shell that uses `app::ShellWindow`, `app::GameScreen`,
`session::GamescopeOverlay` and `launch::Instance`, drawing the Guide scrim and an opaque red panel
at x < 400 over an SDL3 GPU game cleared to (0,255,0). Captures are Gamescope's own full
composition (`gamescope_control.take_screenshot`, type `full_composition`), a game pixel at
(900,400):

| Run | Overlay swapchain | Interactive | Passive | Hidden |
| --- | --- | --- | --- | --- |
| Before | through the layer, `flip: true` | (0,0,0) | (0,0,0) | (0,255,0) |
| After | Xwayland, no layer | (0,196,0) | (0,196,0) | (0,255,0) |

196 is 255 × (1 − 115/255) blended in linear light. After the fix the game still creates its
swapchain through the layer (`flip: true`). X input focus: the overlay window while interactive,
the game window while passive and hidden, before and after.

Tests: `command`, `instance` (the started program has no `DISABLE_GAMESCOPE_WSI` and keeps the
rest) and `compositor_session` (Gamescope gets `DISABLE_GAMESCOPE_WSI=1`) fail before the fix and
pass after.

Check on the desktop: run the desktop shortcut, start any game, press Guide; the game should be
visible, dimmed, beside the menu.
