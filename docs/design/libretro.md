# Built-in libretro runner

Status: phases 1, 2 and 3 built (`opensu-retro` with its control channel and Vulkan GPU cores,
S032); core downloads and launch are S031.

## 1. Goal

Retro systems play inside openSU with one fixed controller mapping in the runner for every core
(a player's button mapping, S033, is applied by the shell to the virtual pad the runner reads),
saves that survive any crash, and the Guide overlay and quick menu working as they do over any
game. A core that crashes ends its game, never the shell.

Non-goals for now: rewind, run-ahead, achievements, cores that draw with OpenGL, GLES or
Direct3D (refused by name; the runner has no GL path), anything a standalone emulator already does
better (Switch, PS2 and later, Wii U, GameCube/Wii, Xbox, Xbox 360).

## 2. Shape

Each game runs `opensu-retro`, a second executable built from this repository, in the game's own
systemd scope like any other emulator. It loads one core with `dlopen`, opens its own window and
runs the core's frame loop. Gamescope shows that window, the shell's overlay goes over it, and the
handoff sees it appear and end exactly as it sees a standalone emulator. On Android, Windows and
macOS the runner has its own window or Activity the same way and the system compositor, not
openSU, puts the shell's panels over it (below).

```
shell ── Handoff (scope) ──▶ opensu-retro --core <so> [options] <rom>
                                 │ SDL window, Vulkan    │ evdev pads (the shell's virtual pad
                                 │ SDL audio             │ while the shell holds the real ones)
                                 └ saves under <data>/opensu/saves/profiles/<profile>/<system>/
```

Why a child window and not frames sent back to the shell: the overlay, scopes, game-quit handling
and orphan cleanup already exist for game windows; a GPU core draws with the child's own Vulkan
device; there is no frame transport to build or to add latency.

**Embedded (Android, Settings › Emulators, off by default).** `prepareRetroLaunch` adds `--embedded`
and `Launches` picks `EmbeddedRunner` over `TaskRunnerHost`: a `SurfaceView` under SDL's
(`setZOrderMediaOverlay`) in the shell's window, its `Surface` passed by `Messenger` to `RetroService`
in `:retro`, where the presenter's `HostedRunnerWindow` draws on it. The process, core, sound and
crash record are unchanged; `:retro` has no Activity, so the shell hears all input and `RunnerRelay`
posts it. `Handoff::Hooks::shown` ends the card on `/status`'s `shown`, not on a display window.
`docs/plans/embedded-runner.md` has the risks and the desktop routes.

The runner does not use the shell's renderer: SDL3 gives it the window, keyboard and sound device, and it draws
with Vulkan directly, so a core using libretro's Vulkan hardware interface
(`retro_hw_render_interface_vulkan`) is handed the runner's instance, device and queue.

**The Guide over the game.** The picture never crosses a process: while a game runs the shell draws
only its panels on a clear (alpha 0) frame, and its window lies over the game's. The game keeps
running under an open Guide, sound included, on its own frame clock; the shell only rests its pads
(`GameScreen`). Save and close is the one thing that stops it.

| Where | How | Game shown |
| --- | --- | --- |
| Linux, Gamescope | `GamescopeOverlay`: the window on a 32-bit visual, `STEAM_OVERLAY` | live, any game |
| Android | `ShellActivity`'s window goes translucent (`Activity.setTranslucent`, under a `windowIsTranslucent` theme) as the runner starts and opaque when the game ends, on a `PixelFormat.TRANSLUCENT` `SurfaceView`; the shell's task comes over the runner's, which Android keeps visible, paused but not stopped | live, the built-in runner |
| macOS | `RaisedWindowOverlay` over a window gpu2d made non-opaque with its Metal layer | live, any game, where the two windows overlap |
| Windows | `RaisedWindowOverlay` over an opaque window; `GameBackdrop` reads the runner's last frame (`GET /frame.png`) as the menu opens and draws it behind the panels | the last frame, the built-in runner; nothing else |

Linux has no other path: an interactive shell always runs in openSU's own Gamescope
(`runInOwnSession`), and only `--hidden` and `--render` run without one, with no window to overlay.

Android stops an opaque window's neighbours, which was the Guide's solid background before: an
opaque `ShellActivity` in front leaves the runner's task invisible and its surface destroyed. The
Library stays opaque, so the launcher behind it can stop. `Activity.setTranslucent` is public from
API 30; below it the shell stays translucent, as the theme says, and the Library's alpha of 1 hides
what lies behind. The shell is over the game only when the game opened it; left by Home or Recents
and brought back, it sends the game forward (`Launches.resume`) rather than show the home screen
behind the menu, and the player opens the Guide from there. Only the game brings the shell forward
and only the shell's resume sends the game, so the two cannot bounce. `AppTask.moveToFront` puts the
home screen right under the task it moves, so the shell and the runner bring each other forward
with `startActivity`: the shell stays right under a game in front, and is what a dying game leaves
on screen. A runner gone for any reason ends its game: `Launches.running` answers from the process
and, while it forks, its task, because a shell stopped under the game may never have seen the pid. A game being closed stays
behind the menu until it has gone (`GameScreen::closing`): sent forward, it would stop the shell,
whose SDL thread then blocks and never sees the game end. A standalone
emulator on Android gets no Guide at all: nothing of openSU's hears its input, and the shell coming
forward means the game is over.

Under the shell the runner is `covered`: it has lost window focus (SDL pauses only on `onStop` from
API 24, and a paused window under a translucent shell is never stopped), the touch
pad hides and lets go of its buttons. While the Guide is open `GuideMenuController` holds a local
game (`POST /hold`, `/release`): the core stops and the sound with it, the picture stays and the
control channel goes on answering, so Save and close and Resume work. A game held on Android goes on
if it comes back in front with the Guide left open (Home and back). A game with guests, one streamed
from elsewhere and a netplay session, which refuses the request, keep running: they cannot pause.
With no surface left (Home, the screen off: `Presenter::onScreen`) the game waits.

Windows is a backdrop because no transparent route works on every GPU there. SDL GPU refuses
`SDL_WINDOW_TRANSPARENT` (`SDL_ClaimWindowForGPUDevice`), because its D3D12 flip-model swapchain
cannot carry alpha, and Vulkan's Win32 surface offers premultiplied alpha per driver: NVIDIA reports
it, AMD's reports `VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR` only
([AMD-Gfx-Drivers#120](https://github.com/GPUOpen-Drivers/AMD-Gfx-Drivers/issues/120),
[SDL#16497](https://github.com/libsdl-org/SDL/issues/16497)). The way to a live overlay on Windows
is DirectComposition. `RaisedWindowOverlay::seesGame` asks the window, so a translucent one drops
the backdrop by itself.

One core per process is also what libretro's callbacks assume: they carry no context pointer, so
the runner's callback trampolines reach the one loaded core through a single pointer in
`retro/core.cpp`.

## 3. Owners

| Owner | Owns |
| --- | --- |
| `cmake/Libretro.cmake` | `libretro.h` and `libretro_vulkan.h` (MIT) from libretro-common at a pinned commit, SHA-256 checked |
| `cmake/Librashader.cmake`, `cmake/thirdparty/` | librashader from the `SomeoneIsWorking/librashader` fork at the commit `cmake/ThirdParty.cmake` pins: its header (MIT, SHA-256 checked), and `librashader-capi` built with cargo (Rust 1.88 or newer; for Windows or an Android ABI cross-built for its target) once into the shared third-party prefix, found there, staged in the build tree as `<libdir>/opensu/librashader.so` and installed; the APK carries it beside the runner |
| `src/retro/port_input.hpp` | What a core's input reads (`PortInput`): `Joypad`, or `NetplayLink` in a session |
| `src/retro/netplay_link.*` | `--netplay`: the session file read (`netplaySetupOf`), this machine's pad into `netplay::Session` each frame, the core's input from it, the system RAM's CRC-32 to compare |
| `src/netplay/` | The session and its transport, no core or LAN knowledge above the seam (`docs/plans/mgba-netplay.md`) |
| `src/retro/core.*` | One loaded core: `dlopen`, the `retro_*` symbols, init/load/run/unload in order, the callback trampolines |
| `src/retro/core_environment.*` | The environment commands answered: directories, pixel format, core options, log, input descriptors (port 0's joypad controls kept, `takeInputDescriptors`), the ports a core lists (`controllerPorts`), geometry and timing changes, the hardware-render commands (through its `HardwareRender`); anything else is refused. Null `data` is answered before any of them (`answerWithoutData`): whether the command is available, nothing written; false where true would say data was taken (`SET_PIXEL_FORMAT`, `SET_SYSTEM_AV_INFO`, `SET_HW_RENDER`); the option commands remove every option; the netplay commands and `GET_HW_RENDER_INTERFACE` answer as they would with data |
| `src/retro/core_options.*` | The core's option definitions (v0 variables, v1, v2) and their current values: the defaults, except a value the launch chose (`--option`) that the core offers for that key |
| `src/retro/hardware_render.*` | A GPU core: `SET_HW_RENDER` (Vulkan taken, any other context refused with its name kept as the load failure's reason), `GET_PREFERRED_HW_RENDER` (Vulkan), the context negotiation interface kept for the device, `GET_HW_RENDER_INTERFACE` (`retro_hw_render_interface_vulkan` on the presenter's instance, device and queue), `set_image`, semaphores, command buffers, the signal semaphore, the queue lock, the sync index; `context_reset` once the presenter is up, `context_destroy` before the game is unloaded; the image of each `RETRO_HW_FRAME_BUFFER_VALID` frame, kept for duplicated frames |
| `src/retro/video_frame.*` | A frame in the core's pixel format (0RGB1555, XRGB8888, RGB565) to RGBA, turned as `SET_ROTATION` asked, duplicate frames kept |
| `src/retro/audio_sink.*` | The core's samples into a ring, resampled with dynamic rate control so the buffer stays near its target |
| `src/touch/` | The on-screen controls (**Touch controls** below): `touch_layer` runs a `ControlPlan` of `touch_controls`, `pad_layout` and `arcade_layout` make one, `touch_art` and `label_face` draw it |
| `src/retro/runner_touch.*` | The runner's touch controls: laid out for the window, fingers taken, what they hold on the joypad as the pad `touch`; an arcade game's panel made again as the core names its controls |
| `src/retro/arcade_cabinet.*` | An arcade machine's panel from the core's descriptors and the listing's `--arcade` |
| `src/retro/joypad.*` | The pads' controls (`gamepad::Button`) and the keyboard to `RETRO_DEVICE_JOYPAD` state, one fixed mapping; the pads' sticks (`gamepad::Sticks`) to `RETRO_DEVICE_ANALOG`; which port each pad plays on (**Input**) |
| `src/retro/save_memory.*` | The core's save RAM: loaded before the first frame (a smaller file into its start; a larger or unreadable one refused and never written over), written atomically when it changes (checked once a second) and at exit |
| `src/retro/audio_output.*` | Where the sink's frames go: the default playback device (SDL audio stream, pulled on SDL's thread), or, hidden, drained by the clock at the output rate with nothing played |
| `src/retro/presenter.*` | The only owner of the SDL window and Vulkan presentation: window events, swapchain (recreated on resize or out-of-date), each frame's command buffer, submission (a GPU core's semaphores waited, its command buffers first, its signal semaphore signalled) and present; hidden, an offscreen image instead of a swapchain, read back for `--frame-png`; the last frame read back from either (`readBack`); whether Android's window still has a surface (`onScreen`); the wait on the last frame a core's `wait_sync_index` is |
| `src/retro/frame_compositor.*` | What is drawn into a target (`change` swaps the video settings between frames, the new preset built before the old one goes): the render pass the targets' framebuffers are made for, the frame texture uploaded each fresh frame or a GPU core's own image sampled in its layout (queue family ownership taken and given back when it names another family, then a barrier so the core's next writes wait for the read), one pipeline drawing it where `placeFrame` puts it for the video settings (aspect, integer scale, overscale clipped to the target) with the chosen sampling and filter, black around it; with a shader preset its chain draws into that rectangle instead, from the texture or the core's image |
| `src/retro/frame_filter.*` | The fragment shader for a sampling and filter, the built-in filters' parameters (`scanlineGrid`, `lcdGrid`, `crtLottes`) and the push block each draw hands the shader |
| `src/retro/librashader_library.*` | librashader opened by its path (`Config::librashader`), ABI and API checked, its calls with their errors as `ShaderError`; a missing file refuses the preset |
| `src/retro/shader_chain.*` | One slang preset built on the runner's device (render passes, one frame in flight) and recorded from the frame image into the placed rectangle; history (cleared on the first frame), feedback and LUTs are librashader's |
| `src/retro/shader_scale.*`, `gpu_timer.*`, `reduced_output.*` | The Shader resolution setting at run time: the preset's GPU time per frame from two timestamps (logged as the `shader` stage), the scale it is drawn at (Full: always 100 %; Auto: 100/85/70/60/50 % against the core's frame interval, each step logged), and below 100 % the image the chain draws into and its bilinear draw into the placed rectangle |
| `src/retro/vulkan_device.*` | The Vulkan instance (validation layer and its error count on request) and the one device and graphics queue, made through a GPU core's context negotiation when it has one (its `get_application_info`; `create_instance`, `create_device2` or `create_device` with the runner's extensions and layers added), `destroy_device` before the instance goes; the queue's lock, held for every submit, present and device wait |
| `src/retro/png_encoder.*` | RGBA to PNG bytes through libpng |
| `src/retro/shaders/frame.{vert,frag}`, `sharp_bilinear.frag`, `frame_grid.glsl`, `crt_lottes.frag` | The frame's shaders (sharp-bilinear: integer prescale then bilinear; `frame_grid.glsl`: the scanline and LCD-grid bands both sampling shaders apply; crt-lottes), compiled by glslc at build time into SPIR-V headers under the build tree |
| `src/retro/save_state.*` | The one state slot `<saves>/<rom stem>.state`: `retro_serialize` written whole, read back through `retro_unserialize` |
| `src/retro/resume_file.*` | The resume state's format, shared with the shell (`opensu::retro_saves`): `opensu-resume 1`, a JSON line (core name, its label and version, when, the save RAM's CRC-32), the core's state; written whole, its header read alone, removed |
| `src/retro/resume_state.*` | Save and close's state taken (`suspend`) and put back before the first frame (`resume`) under the checks in **Save and close** below |
| `src/retro/control_server.*` | The runner's loopback `lucent::http::Server`: the control file (port and pid, or the load failure), the routes, the hand-over of each command and of the control pad's input to the frame loop |
| `src/retro/control_commands.*` | What each queued command does to the game between frames, Save and close's stop included |
| `src/retro/frame_stages.*` | Each frame's stages timed (core, audio, fence, acquire, record, present, sleep) and logged with the frame time and audio queue as p50/p95/p99 every 5 s |
| `src/retro/runner_app.*` | Composition: core and owners, presenter, pads, keyboard, sound, the loop and its pacing, save checks, control commands between frames, the hidden mode for tests |
| `src/retro/main.cpp` | `opensu-retro`'s command line, the `--settings` file read into `settings::VideoSettings` |
| `src/settings/setting_layers.*` | The setting layers (Global, System, Emulator — the core layer —, Game) and the one resolution rule, each value with the layer it came from |
| `src/settings/video_settings.*` | `video.aspect`, `video.integer`, `video.sampling`, `video.filter`, `video.shader` (a path in the pack, `""` for None; a preset replaces sampling and filter): their choices and labels, resolution to typed values, the per-launch settings file and `POST /video`'s body (`videoJson`, `parseVideoJson`) |
| `src/settings/game_settings_store.*` | `<config>/opensu/game-settings.json`, every layer of every key |
| `src/settings/emulator_pick_migration.*` | The one-time move of `settings.json`'s `emulators` and `systemEmulators` into the game settings |
| `src/app/game_settings.*` | The layers as the shell holds them, saved with a toast on failure; the words a source reads with |
| `src/app/retro_launch.*` | A launch on a built-in core: its video settings resolved, written to the launch's settings file, `--settings` added; the core options the layers keep as `--option` |
| `src/settings/setting_schema.*`, `setting_scope.*`, `core_option_settings.*` | Every layered setting declared once (video, shader preset, frame generation, resolution and upscaling, the emulator pick, a core's options), validation and clear from it; a scope's rows with the value in effect and its source |
| `src/app/layer_settings.*` | Settings › Emulators' Global, Consoles and Built-in cores rows, each opening the layered editor |
| `src/app/layered_settings.*`, `layered_editor.*` | The one layered editor (any scope; a page or a panel over the game) and what it edits, each change saved and applied to the running game |
| `src/app/core_option_store.*` | Each core's option definitions as last seen, for editing while nothing runs |
| `src/app/shader_preset_tree.*` | The shader pack's folder tree and a preset's label; the picker is `ShaderPresetScreen` in `layered_editor` |
| `src/library/shader_pack.*` | libretro's `shaders_slang.zip`: the latest build (HEAD: `Last-Modified` and size), the download checked by length and each member's CRC-32, unpacked into `<data>/opensu/shaders/slang/` with its record, removal, the presets it holds |
| `src/library/libretro_cores.*` | Finding a core (openSU's downloads, RetroArch's and the system's folders) and its info file; core downloads from the libretro buildbot under `<data>/opensu/cores/`, with their info and record |
| `src/library/core_bios.*` | A core's BIOS files from its core info, what it does without them, which are in the system folder |
| `src/library/romset_dat.*` | An arcade core's ROM-set listing: a Logiqx datafile or MAME `-listxml` parsed (a set's `<input>` among it, `DatControls`), its line format under the cache |
| `src/library/romset_dats.*` | The arcade cores' listings fetched over HTTPS from the catalog's `"romsets"` and kept under `<cache>/romsets/` for 30 days |
| `src/library/romset_lists.*` | Those fetches off the library scan, one at a time on a thread, each one's state (bytes, done, failed and when it is tried again) for the Downloads page |
| `src/library/romset_match.*` | An arcade zip's central directory, read through the `RomTree` that listed it, against each listing, the verdicts kept by file stamps, the game's emulator picked by them, a picked folder's parent and BIOS zips added to the launch |
| `src/library/romset_conversions.*` | Recipes, aliases and CPS2 keys from `assets/`, and the plan that turns held zips into a listing's set. |
| `src/library/romset_convert.*` | That plan built at launch into a cached zip, checked by CRC-32. |
| `src/library/emulator_choice.*` | The `emulator` key at the Game and System layers: game pick, romset, system pick, search; the layer keys a game reads its settings through |
| `assets/emulators.json`, `src/library/emulator_catalog.*` | A built-in emulator is an entry with `"core": "<name>_libretro"` and no launchers or release; `"coreOptions"` the core options it starts with (a GPU core's Vulkan renderer) |
| `src/library/emulators.*` | A built-in emulator is installed when its core is found and `opensu-retro` is beside `opensu` (`config::retroRunnerBeside`); it starts as `opensu-retro --core <core> [--option <key>=<value>]... <rom>`; a hidden shell adds `--silent` inside a Gamescope and `--hidden` elsewhere (`RunnerMode`) |
| `src/app/emulator_downloads.*`, `emulator_settings.*`, `details_controller.*` | Download, update check, update and removal of a core and of the shader pack, on the same paths as an AppImage; the Shader presets rows; the details page's video sources, Game settings button and Clear |
| `src/library/linked_play.*`, `src/app/lan_netplay.*` | A game on a core that links its system (`"links"`): split screen and netplay commands (`linkable`, `splitScreenGame`, `netplayGame`, `sessionCore`); the shell's half of a session (Play with <name>, the question, saves, the session's file) |
| `src/app/retro_session.*` | The shell's side of the control channel: each launch's control file, the port read from it, the requests (`setVideo` for `POST /video`), the load failure handed over and the file removed |
| `src/app/game_backdrop.*` | The game's last frame behind the menu where the window cannot show it (`GameOverlay::seesGame`) |
| `src/app/guide_menu_controller.*` | Save and close, Game settings, Save state, Load state and Reset game while the running game answers on a control channel; the resumed or refused row |
| `src/app/game_end.*`, `src/launch/handoff.*` | How a game ended (`Hooks::ended`, the scope's exit status) and the toast it gets |

`retro` depends on `gamepad`, `fileio`, Lucent, nlohmann/json, SDL3, the Vulkan loader and libpng;
`config` is read by `main.cpp` only. Nothing outside `retro` and the `opensu-retro` executable links
it. Only `presenter`, `frame_compositor`, `shader_chain`, `gpu_timer`, `reduced_output`,
`librashader_library`, `vulkan_device`
and `hardware_render` (and through it `core_environment`) include Vulkan, only the last two
`libretro_vulkan.h`, and only `presenter` uses SDL video; `audio_output` includes SDL audio;
`joypad` uses SDL's scancode values only. `netplay` (below) links `opensu::datagram` and `opensu::secrets`, not
`stream`, so the runner carries no PipeWire or codecs.

## 4. Contracts

**Command line.** `opensu-retro --core <path.so> [--saves <dir>] [--system <dir>] [--settings
<file>] [--option <key>=<value>]... [--players N] [--netplay <file>] [--arcade <buttons>:<ways>] [--hidden | --silent] [--touch <dp>] [--validate] [--frames N] [--frame-png <file>]
<rom>`. `--arcade` says the game is an arcade machine and what its set's listing says of its controls (**Touch controls**). `--option` sets a core option for this run: the core's value for that key when it offers
it, else (a warning logged) its default. `--settings` names a JSON
object of resolved video settings (`{"video.aspect": "4:3", "video.integer": "off",
"video.sampling": "nearest", "video.filter": "none", "video.shader": "<absolute .slangp path>",
"video.shader-resolution": "auto"}`);
a key it lacks keeps its default, a value that is no choice is exit 1. The runner never reads `game-settings.json`. `--saves` defaults to
`<data>/opensu/saves/<core name>/` (the core file's stem without `_libretro`; the launcher passes
the active profile's system folder), `--system` to `<data>/opensu/system/`, both from `config::Config`. `--hidden`,
`--validate`, `--frames` and `--frame-png` are for tests and tools: `--hidden` is an unmapped SDL
window (`SDL_WINDOW_HIDDEN`) whose Vulkan surface the core's device is made for (Dolphin builds its
frontend swapchain from it) but never presented to, drawing into an offscreen image, no sound device (the sink is drained
by the clock, so rate control still runs), no pads and no keyboard. `--silent` is a hidden shell's
game inside its Gamescope: no sound device and no pads, as hidden, but the window is drawn and
presented as the player's is, fullscreen on that Gamescope's surface. A window inside a Gamescope
reaches that Gamescope's screen alone, so the hidden rule (nothing on the player's desktop) holds
while a tool run takes the player's present path; the sound device and the pads are the player's
in any session, so they stay off. `--touch <dp>` (hidden only) shows
the touch controls at that many pixels per dp, as Android's window does, for `POST /touch`; `--validate` enables
`VK_LAYER_KHRONOS_validation` when installed and counts its errors; `--frames` stops after N frames;
`--frame-png` (hidden only) writes the last image shown, letterboxing included, at the window's
size (`OPENSU_WIDTH` × `OPENSU_HEIGHT`, 1280 × 800 by default). Inside Gamescope the window is
fullscreen.

**Exit status.** 0 when the player quits (window closed, Escape, SIGTERM or SIGINT, which SDL turns
into a quit so save RAM is written; the shell's Close game stops the scope, which is SIGTERM) or
the frame count is reached; 1 for a bad command line or a
window, Vulkan, shader preset or frame-file failure (the reason also goes to the control file,
so the shell toasts it); 2 when the core or game cannot be loaded (the reason logged);
3 when `--validate` saw validation errors; a crash is the signal it died of. The shell learns how a
game ended from the scope: the handoff reports the started process's status (`Instance::exitStatus`,
128 plus the signal) through `Handoff::Hooks::ended`, except for a game force-closed from a menu.
On Android `Launches` reads the runner's end from `ApplicationExitInfo` (API 30+, older ones report
a plain end), waiting up to 3 s for the record, and a crash with no signal of its own counts as
SIGABRT. A death by any signal but SIGTERM, SIGKILL, SIGINT and SIGHUP is toasted as `<title> crashed
(SIGSEGV)`; a load failure the runner left in its control file is toasted as its reason instead.
Other ends keep the launch's own toast (`<title> closed`, or its failure).

**Control channel.** The shell sets `OPENSU_RETRO_CONTROL=<runtime>/opensu/retro-<shell pid>-<n>.json`
(`<runtime>` is `$XDG_RUNTIME_DIR`) in every launch's environment; `config::Config::retroControl` is
how the runner reads it. With it set, once the game has loaded and the window is up, the runner
listens on a free loopback port (`lucent::http::Server`, port 0) and writes
`{"port": N, "pid": P}`, with `"netplayPort"` for a session's bound UDP port, to that file atomically (`fileio::writeWhole`); it removes the file when it
exits. When the core or game cannot be loaded (exit 2), the presenter or its shader preset
cannot be set up (exit 1), or a frame cannot be drawn during the game (exit 1, "the game stopped
drawing: …", a lost device among them), it writes `{"error": "<reason>"}` there instead and leaves
it (`ControlServer::reportFailure`); the shell reads it once the launch is over, shows it as an
error and deletes it. Routes:

| Route | Answer |
| --- | --- |
| `GET /status` | `{"core", "rom", "frame", "hasState"}`: the core's name, the ROM path, frames run, whether the slot holds a state; `resumedFrom` (Unix seconds) when the game resumed, `resumeRefused` (the reason) when its resume state was not loaded |
| `POST /hold`, `POST /release` | The Guide came over the game and left: frames and sound stop, the picture stays. 409 in a netplay session |
| `POST /suspend` | Save and close: 200 once the save RAM is written and the resume state taken, after which the runner runs no more frames and waits to be closed; 409 when the core cannot serialize; 500 when either file cannot be written, the game going on |
| `POST /state/save` | 200; 409 with the reason when the core cannot serialize (`retro_serialize_size` 0 or `retro_serialize` false); 500 when the slot cannot be written |
| `POST /state/load` | 200; 404 when no state is saved; 409 when the core refuses it |
| `POST /reset` | 200 after `retro_reset` |
| `POST /turn` | 200 once Rotate screen changed (`ScreenTurn::toggle`), the Guide's row |
| `POST /video` | 200 once the video settings in the body (`settings::videoJson`, the `--settings` file's format) are drawn from the next frame; 400 for a body that does not parse; 409 with the reason when the shader preset cannot be built, the settings drawn before kept |
| `GET /frame.png` | The last frame shown as PNG, letterboxing included; a swapchain's is shown once more to be copied; 500 before the first frame or while the window has nothing to draw into |
| `POST /touch` | 200: `{"finger": id, "phase": "down"\|"move"\|"up", "x": px, "y": px}` in the target's pixels, put on the window's event queue as an SDL finger event, so it takes the path a real finger does; 400 for anything else |
| `POST /host` | 200: what a hosted game cannot see of the shell, any of `{"covered": bool}`, `{"pads": n}`, `{"key": {"scancode": n, "down": bool}}`, `{"screen": {"safeX", "safeY", "safeWidth", "safeHeight", "dp"}}`; 400 for anything else. A hosted game starts covered |
| `GET /touch` | Where the touch controls are: `[{"control": name, "x", "y", "radius"}]`, each button by its `gamepad::name`, the stick as `stick` with how far a finger reaches it |
| `POST /input` | 200: a control pad holds `{"pad": name, "buttons": [names], "left": [x, y], "right": [x, y]}` (each part optional, sticks from -1 to 1, y down) until its next post, `{}` letting go, `{"pad": name, "leave": true}` leaving; without `"pad"` it is the one control pad, each name a pad of its own; 400 for anything else or Guide |

Every route but `GET /status`, `/touch`, `POST /input` and `POST /host` is queued for the frame loop, which runs it between frames, never beside `retro_run`;
the request waits for the answer, and is answered 503 when the loop has not taken it within 3 s or
the runner is closing. Error bodies are the reason as plain text. The slot is
`<saves>/<rom stem>.state`, written whole, so a crash mid-write keeps the previous state. The shell
reads the port lazily and talks through `lucent::http::get_loopback` and `post_loopback`; requests
run on the shell's loop, which a runner that stops answering holds for up to the client's 5 s
socket timeout.

**Guide menu.** While the running game answers `/status`, the Guide menu over it shows Save and
close below Close game, then Save state (one press), Load state (asking first; an Info row reading
"No state saved" when the slot is empty) and Reset game (asking first). The results are toasts:
"State saved", "State loaded", "Game reset", or the runner's reason as an error. Its "Game
settings" row opens the layered editor (below) as a panel over the game; B from it comes back to
the menu on that row. A game that resumed shows "Resumed session" with the date as its
caption; one whose resume state was refused shows "Not resumed" with the reason.

**Save and close.** Save and close posts `/suspend`, then closes the game the way Close game does
(the scope stopped, SIGTERM). The runner writes the resume state
`<rom stem>.resume` (`retro::resumeStateFile`) in the profile's system folder beside the `.srm`,
never the player's `.state` slot, so each profile has its own, then the save RAM, all of it even
when no frame changed it (`SaveMemory::keep`), and runs no
more frames, so the state and the `.srm` are of the same frame; a refusal keeps the game running
and is toasted. The next start of the game, on any launch, loads it once the core has the game, its
save RAM and (a GPU core) its context, before the first frame, and removes it. It is loaded only
when the core's name (`coreName`) is the one that wrote it and the save RAM is the one it was taken
with (CRC-32), so a save copied, synced or made since is never undone by an older state. A state
another core wrote is kept for that core and reported; one the core refuses, or whose save RAM has
changed, is reported and removed, and a refusal resets the core with the save RAM it booted with in
case the core took part of the state. Nothing of it crashes the start: each case is a log line,
`resumeRefused` and the Guide's "Not resumed" row.

Removing it on load is the choice for a crash: a game that crashes after resuming starts next time
from its own save, where keeping the state until the next Save and close would put the player back
at an older point each start and, once the game had saved, could only be refused. Plain Close game
keeps no resume state: its confirm says unsaved play is lost, and a setting that made it keep one
would make Close mean two things. The details page reads the header (`retro::readResumeHeader` on
`library::saves::runnerResumeFile` for the active profile): a Resume fact "Resumes from <date>", or "Kept by <core>, which
<game> does not start on" when the game's core is another, and Start fresh, which asks first,
removes the state and starts the game.

**The resume state travels with the save.** It is a save slot (`SlotKind::Resume`), so Copy saves
and every sync target carry it. A bundle holds it only beside the save RAM whose CRC-32 its header
names (`save_sync::bundleOf`, `retro::resumeGoesWith`): the state is written before the `.srm`, so
a sync reading between the two sends the old save alone, and a state the save moved on from stays
here. Save RAM taken without a state removes the state here (`SaveCopier::copy`, kept in the
backup): one machine's resume, which removes its file, reaches the other as a bundle without it at
that machine's next sync, and the file is gone on both. The checks above run on the machine that
loads it. A game with no save RAM keeps its state on the machine that wrote it: with no file left
after the resume, the rule that no save takes the other side's would bring the state back. A
standalone emulator keeps no resume state, so a peer launching the game in one takes the save
alone, and its next sync removes the state here.

**Input.** The runner reads every evdev pad without grabbing it. While the shell holds the real
pads during a game, the only pad that moves is the shell's virtual Xbox 360 pad, so the Guide
chords stay the shell's and every core sees the same layout. Run alone, it reads the real pads
through the same translator (`gamepad::Pads` with `OwnVirtualPads::Read`, never holding). Pad
positions follow the Xbox layout: A (bottom) is libretro's B, B is A, X (left) is Y, Y is X; L1/R1
are L/R, L2/R2, L3/R3, Select, Start and the d-pad and left stick are themselves; Guide reaches no
core. Keyboard: arrows, Z (B), X (A), A (Y), S (X), Q/W (L/R), Enter (Start), Right Shift (Select);
Escape quits. With `--players 1` (the default) every pad, the keyboard, the touch layer and the
control pads are port 0. With `--players N` each pad (a real or virtual evdev pad, the touch layer,
each named control pad) takes the lowest free port of the N when it first presses a button or
moves a stick, keeps it until it leaves, and a pad beyond N plays on none; the keyboard is port 0.
In the shell a Game claim gives each real pad its own virtual pad, so each player's pad is a port
of its own, and a launch from the shell has `--players 5`: the player here and remote play's four
guests, each guest's `RemotePad` one more evdev pad. An analogue trigger is L2 or R2 past half its travel
(`gamepad::triggerPressAt`). Both sticks are also `RETRO_DEVICE_ANALOG` (`gamepad::Pads::sticks`,
on the virtual pad's -32768 to 32767, y down; of several pads, each axis the one furthest out);
`RETRO_DEVICE_INDEX_ANALOG_BUTTON` reads 0. The control channel's pad (`POST /input`) is one more
source beside the pads. Under `--netplay` the core reads `NetplayLink` instead (next section). There is no mouse, so `RETRO_DEVICE_POINTER` reads 0: melonDS DS's touch
screen is its joystick mode, the right stick moving its cursor and R2 touching.

**Touch controls.** Where the runner draws its own (`shell_handoff`, or hidden with `--touch`) the
controls show while no pad is connected and nothing covers the game. Each finger keeps the control
it went down on, so a stick held while buttons are pressed is several controls at once; what they
hold is one more pad, `touch`. A press stays on that pad for frames the core ran, whenever its
finger lifts (`RunnerTouch::ran`): one, and four for Select and Start, because a CPS2 game drops a
one-frame coin and takes a two-frame one. Every system gets the generic pad (`pad_layout`) except
arcade.

An arcade game's launch has `--arcade <buttons>:<ways>` (`app/retro_launch`, from its core's listing;
`0:0` where the listing says nothing; `v` before a vertical stick's ways, `f` after them for a
fighter's board) and shows its machine's
panel (`arcade_cabinet`, `arcade_layout`). What each core sends, checked by running them:

| Core | `SET_INPUT_DESCRIPTORS` | Listing's `<input>` | The panel relies on |
| --- | --- | --- | --- |
| FinalBurn Neo | Per game, in the first `retro_run`: Attack, Jump; Weak Punch...; Button A-D; Fire 1. Macros on L/R, L2/R2 ("Buttons ABC", "Buttons 3x Punch"), Neo Geo's Select on L3 | None: its datafiles are Logiqx | The core |
| FB Alpha 2012 | Per game, the same names, but only from `retro_set_controller_port_device` once every port it lists has a device; so `Core::loadGame` sets one on each port of `SET_CONTROLLER_INFO` | None | The core |
| MAME 2010 | One table at load for every game: Button 1-7, Turbo Button, Service/Test, Toggle MAME UI | `buttons` and `joy8way`-style `<control>` | The core for which id each button is, the listing for how many and the stick's ways |

So descriptors come first. A core whose action buttons are all `Button <n>` says nothing of the game
and is cut to the listing's count. The stick's ways are the listing's, else 8 with both axes named,
2 with one, none with neither. L3 and R3 are never panel buttons, and "Buttons ...", turbo, service,
diagnostics, reset, test and tilt are left off. With nothing from the core a listing's count of
buttons is numbered on the RetroPad's B, A, Y, X, L, R, L2, R2; with nothing at all it is an 8-way
stick and four buttons. SELECT is Coin and START is Start on every arcade core, so both are always
there, small and labelled, and pause holds Guide. A descriptor's word is the button's label ("Button
A" reads A). The rows are by count (`rowsByCount`): one row up to four, which is Neo Geo's, 3+2, 3+3
for Capcom's six, then 4+3, 4+4, each column on an arc. FinalBurn Neo names its controls after the
first frame, so the panel is made from the listing first and again then.

A button the core calls a punch or a kick shows a fist or a foot (`assets/touch/fist.svg`,
`foot.svg`) over its strength, L, M or H (`arcade_cabinet`: Weak, Light, Low; Medium, Mid, Middle;
Strong, Heavy, Hard, High, Fierce; a plain Punch has no letter, and a named move such as Hurricane
Kick keeps its name). When every button is one, they run punches above kicks, weakest first. A
core with numbered buttons says nothing of this, so the listing does: six buttons on driver `cps1`,
`cps2`, `cps3` or `sf` (`romset_dat`'s `DatControls::fighter`) read as three punches then three
kicks. Against MAME 2010's listing that is 213 sets of 28 games, bootlegs included. It wrongly
catches JoJo's two games (seven sets), whose fourth button is the Stand. The manufacturer would be
worse: NAOMI, ZN and ST-V list six for the cabinet, so it catches Giga Wing 2 and Cannon Spike.

The stick goes on the left of the picture, the buttons and Coin and Start on the right: inside the
black bar when they fit (`Presenter::picture` is where `placeFrame` puts the frame, followed as it
changes), else against the screen's edge, translucent where they cover the picture. A thumb's size
comes before the bar: the stick is never under 168 dp, and a row takes what its bar gives down to
52 dp, or 13 mm (84 dp) when the bar cannot hold it at that, short of running under the stick on an
upright screen. A row is never turned to fit a bar: a 2800x1840 tablet at 2.2 pixels per dp has 79
dp of black beside 4:3, so its stick reaches about 95 dp over the picture and a three-button row
about 200 dp, with Coin and Start small on the bar.

`touch.size` (Touch controls size, 75 to 175%, under Video in every scope) scales every control of
the pad and the panel together, by scaling `dp` in `TouchLayer::layout`; each layout then caps `dp`
(`WindowSurface::dpWithin`) so its controls stay on the safe area. It travels with the video
settings: the launch's `--settings` file and `POST /video`, so it changes in a running game from
the Guide's Game settings. Fingers down at that moment are let go.

On an upright target (`WindowSurface::upright`) neither layout looks for bars: the controls stand
in a band along the bottom, pause and Rotate screen above the stick and Coin and Start above the
buttons on a panel, a row of L, pause, Rotate screen and R and a row of Select and Start above the
d-pad and faces on the pad. `ControlPlan::band` is its height, at most `uprightShare` of the screen
since `dp` shrinks to keep it there, and while the controls show the runner keeps the picture off it
(`FrameCompositor::reserve`). A wide picture stands at the top of an upright target, the controls
under it, and a vertical one centred (`display::standIn`). A landscape target has no band, so
nothing changes there.

**Rotate screen.** `ScreenTurn` owns whether it is on. The touch button beside pause (`Button::Rotate`,
which is no pad's and reaches no game; `TouchLayer::offerRotate`, so a remote game's pad has none),
F9, `POST /turn` from the Guide's row and `video.rotate` through `POST /video` all end in
`ScreenTurn::set`. Nothing rotates by itself: a vertical game starts upright on the landscape screen
like any other (`SET_ROTATION` turns its frame in `VideoFrame`), and only a press changes that.

With `shell_handoff` it asks for the fixed `SCREEN_ORIENTATION_PORTRAIT` or `_LANDSCAPE`
(`LandscapeActivity.turnScreen`), kept for SDL's `setOrientationBis`; a game that never pressed it
stays where the shell rests: `sensorLandscape`, or `sensorPortrait` when the player chose Portrait
in the Guide's Screen row (`screen_orientation::stand`, `LandscapeActivity.standShell`). The native call lands on whatever is the runner's context.
`RetroActivity` turns itself and tells the shell; the embedded `RunnerContext` has no window and only
tells the shell. Both use the Messenger `ShellActivity.runnerInbox`, `RetroService.TURN`, which
`RetroActivity` gets as a launch extra and `RetroService` as `START`'s `replyTo`. So the shell always
stands as the game does, and the Guide over a portrait game stays portrait in both modes, the shell's
pages laying out for that window. A host's `ended` puts the shell back where it rests
(`endTurn`). `/host` only carries shell to runner, so it has no part in this; F9 from the shell's
keyboard reaches a hosted game as a `/host` key like any other. Elsewhere it turns the picture a quarter
clockwise for a rotated monitor or cabinet (`FrameCompositor::turn`): `placeFrame` places the frame in
the extent swapped, `frame.vert`'s `turned` constant turns the texture coordinates, and a shader preset
draws into `reduced_output`'s image at scale or below, since librashader cannot turn its output, which
is then drawn turned.

The choice is `video.rotate`, kept per game. The shell cannot hear the runner on Android, so a press is
written to the control file as `"turned"` (`ControlServer::noteTurned`), and the file stays when the
runner exits rather than being removed. `LayeredSettings::keepScreenTurn` reads it through
`RetroSession::screenTurned` when the Guide opens, after its row and when the game ends, and keeps it at
the Game layer if it differs from what the layers resolve to. The next launch reads it from `--settings`
like any video setting; set in Global or a system's scope it applies to more than one game.

Art and words are rasterised at the drawn size. To check:
`opensu-retro --hidden --touch <dp> --arcade <b>:<w>` with `OPENSU_WIDTH` and
`OPENSU_HEIGHT` as the screen, `GET /touch`, `POST /touch`, `GET /frame.png`; the log's `arcade panel:`
and `touch holds` lines.

**Netplay.** `--netplay <file>` makes this runner one machine of a session. The file is owner-only
JSON: `{"slot": 0, "players": 2, "delay": 2, "ahead": 6, "port": 0, "peer": "192.168.1.2:4000", "clockMs": <Unix
ms>, "utcOffset": <seconds east of UTC>, "sendKey": hex, "receiveKey": hex}`, a file without the clock
refused; port 0 binds one of the system's choosing, reported in the control file, and without `peer`
the runner learns it from the first datagram that opens. Every
machine runs every player, so each must load the same ROM, core and options. Each frame the runner
reads its pad (all of it, as port 0) for frame `f + delay`, sends it repeating the last 16 frames,
and runs frame `f` only when every slot's input for it is in, resending every 8 ms while it waits.
Every 120 frames each side sends the
CRC-32 of `RETRO_MEMORY_SYSTEM_RAM`, not `retro_serialize`: mGBA's state holds its creation time. A
core with no system RAM compares the CRC-32 of its state. `GET_NETPLAY_CLIENT_INDEX` answers the slot and the link
core's own `GET_NETPLAY_CLOCK` (256 | experimental, `retro::getNetplayClock`) the file's `clockMs` and
`utcOffset`, which the core asks for at load: each GBA's clock then runs from that time with the
emulated frames (`RTC_FAKE_EPOCH`) and shows it in that zone, whatever the host's clock and zone, so
a game with a real-time clock reads the same on every machine. The session ends
on Leave (SIGTERM or quit), on 5 s of silence (20 s before the first frame) or on a differing CRC;
the runner logs `netplay: <how> after <n> frames, <n> stalls, <n> frames run again, <n> state checks matched`, writes its
save RAM and exits 0. Load state, reset, a core option's change and Save and close answer 409: one
machine alone would leave the other. In the shell `app/netplay_run` writes the file and runs the
session (`docs/design/lan.md`, `docs/design/online.md`).

**Rollback.** With `ahead` above 0, `NetplayLink::attach` settles how the session runs once the core
is loaded. Lockstep, with `ahead` added to `delay`, when the core gives no state, draws on the GPU or
sets a serialization quirk that a state a few frames old cannot survive (`CoreEnvironment::canRollBack`:
incomplete, must initialize, core variable size, single session, endian or platform dependent); every
machine runs the same core, so all work out the same. Otherwise player 1 offers the size and SHA-256
of its first state (`netplay::StateSync`): a machine holding the same state takes nothing. One that
does not takes it only at this core's own `retro_serialize_size`, allocated then, and loads it once
the whole hashes to what was offered. From there the runner keeps a state per frame in a ring of
`ahead + 2`, may run `ahead` frames past the input it has on a repeat of each player's last one, and
when real input differs (`Session::takeWrong`) unserializes and runs the frames again through
`Core::runQuiet`, picture and sound thrown away. Checksums wait until their frame's input is final.
`GET_SAVESTATE_CONTEXT` answers `ROLLBACK_NETPLAY` throughout a session, as these states are ones
the header says may cross machines, and `NORMAL` otherwise.

**Video settings.** Aspect (`core`: the core's `aspect_ratio`; `4:3`; `pixel`: 1:1 pixels;
`stretch`: the whole target), integer scale (`off`; `on`: the largest whole multiple of the frame's
height that fits, width from the aspect; `overscale`: the next multiple up, clipped to the target)
sampling (`nearest`, `bilinear`, `sharp-bilinear`) and filter, a pass over the placed frame
(`none`; `scanlines`: a band 0.5 source pixels high at the bottom of every source row, taking half
its light; `lcd-grid`: a band 0.25 source pixels wide at the right and bottom of every source pixel,
taking 45 %; `crt`: crt-lottes at its defaults, which does its own sampling, so `sampling` does not
change it). The scanline and grid bands follow the source pixel grid at any scale: an output pixel
is darkened by the share of its footprint they cover, so a band always sits on a source pixel edge
and the picture's mean brightness does not depend on the scale. They are kept in the setting layers
(`settings/setting_layers`, `<config>/opensu/game-settings.json`): a game's own value, else its
core's (the `Emulator` layer, by catalog id), else its console's, else Global's, else the
default; a layer keeps only what it sets and clearing deletes the key. At launch the shell
(`app/retro_launch`) resolves them for a game on a built-in core, writes
`<runtime>/opensu/retro-<shell pid>-<n>.settings.json` and starts the runner with `--settings`; the
file is removed when the session ends. A launch whose file cannot be written does not start.
Settings › Emulators opens the layered editor for Global, each console (its emulator pick too) and
each built-in core; a game's details page shows the source of each value a layer keeps.

The layered editor (`app/layered_editor` over `app/layered_settings`, from Settings › Emulators,
the details page's "Game settings" button and the quick menu's row) has a tab per scope (Global,
the console, the core, the game), LB/RB between them. Each row shows the value in effect with a chip
naming its layer ("Global", "Game Boy Advance", "mGBA", "This game", "Default"); choosing sets it at
the tab's scope; the first entry of every list is "Use <value> from <layer>", which deletes the
scope's key (Y too; Y again restores it during the session); X goes to the tab the value comes from.
Shown-when conditions (Sampling and Filter under a shader preset, the frame generation detail rows
under Custom) read the values in effect at the tab's scope. The core's own options are rows of the
same editor at the core's and the game's layers, as `core.<option>`; a launch passes them as
`--option`, and a running core gets a change over `POST /option`. Each change is saved at once and,
when that game is running, resolved again (`retroVideoOf`) and sent as `POST /video`, so it shows
on the next frame. To check: `opensu-retro --hidden --settings <file> --frames N --frame-png <png>`
and compare the picture's box in the PNG; for a filter, divide its frame by the `none` frame inside
the box and compare each pixel with 1 − shade × band coverage.

**GPU cores.** A core asking for Vulkan (`SET_HW_RENDER`, `RETRO_HW_CONTEXT_VULKAN`; the runner
answers `GET_PREFERRED_HW_RENDER` with Vulkan, which cores whose renderer option is "auto" go by)
gets the runner's one device: the presenter is made after `retro_load_game`, and when the core set
a context negotiation interface (`GET_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE_SUPPORT` answers
version 2) the instance comes from its `create_instance` and the device from `create_device2` on
the runner's chosen GPU (again with no GPU if it declines), else `create_device` (v1, a zeroed
feature struct, the swapchain extension when there is a window); the runner's own extensions and
layers (validation) are added in the wrappers. Then `context_reset`. `GET_HW_RENDER_INTERFACE`
gives `retro_hw_render_interface_vulkan` version 5 with one frame in flight: `get_sync_index` is
0, the mask 1, and `wait_sync_index` waits for the last frame the presenter submitted, which is
also what the presenter waits for before recording the next. A `RETRO_HW_FRAME_BUFFER_VALID`
refresh shows the image of the last `set_image` (`width` × `height`, the whole image), sampled in
the layout the core gave, its semaphores waited once at the fragment stage; a null refresh shows
the same image again without them; `set_command_buffers` are submitted once before the runner's,
and then the semaphores are not waited; `set_signal_semaphore` is signalled by the next
submission. A null `set_image` withdraws the image (LRPS2 does so in `context_destroy`, before
destroying it): nothing is shown from it again. The core's frame goes through the same `placeFrame`, sampling and filter as a
software frame, or the same shader preset: an image in `SHADER_READ_ONLY_OPTIMAL` is the chain's
input as it is (librashader copies it into the history, which the `TRANSFER_SRC` usage libretro
requires of `set_image` images allows, and gives it back in that layout); one in `GENERAL`, which
the runner may not move, is blitted into the frame texture first and the chain reads that. Every submit, present and device wait holds the queue lock the core's
`lock_queue` takes. At exit: the last frame read back, then `context_destroy` (GPU idle), then
`retro_unload_game`, during which the interface's queue lock and sync calls still work (Azahar's
renderer submits as it shuts down; RetroArch keeps its driver up until after the core), then the
device (`destroy_device`, `vkDestroyDevice`) and the instance,
then `retro_deinit`. A core asking for OpenGL, GLES or Direct3D is refused; the load fails with
`<core> cannot load <rom>: it asks for OpenGL core 3.3, and openSU draws with Vulkan only`, which
the shell toasts. Cores whose renderer is an option get it from the catalog's `"coreOptions"`:
Mupen64Plus-Next `mupen64plus-rdp-plugin=parallel`, `mupen64plus-rsp-plugin=parallel` (its default
is GLideN64, OpenGL); ParaLLEl N64 `parallel-n64-gfxplugin=parallel`,
`parallel-n64-rspplugin=parallel`; Beetle PSX HW `beetle_psx_hw_renderer=hardware_vk`; SwanStation
`swanstation_GPU_Renderer=Vulkan`; PPSSPP `ppsspp_backend=vulkan`; Flycast has no renderer option
and picks its context from the preferred one (PPSSPP and Flycast not yet run here: no PSP or
Dreamcast games). To check: `opensu-retro --hidden --validate --option ...
--frames N --frame-png <png>` logs `the core draws with Vulkan` and `drawing on <GPU>, the device
made by the core`, and exits 3 on any validation error.

**Pacing.** Frames run at the core's rate on a clock (`sleep_until`; more than four frames behind,
the clock restarts rather than bursting); the swapchain presents in mailbox, else immediate, else
FIFO mode, so presenting does not hold the clock to vblank. Audio goes out at 48 kHz stereo: the
core's samples are linearly resampled by a ratio within ±0.5 % of nominal, proportional to how far
the ring is from its 40 ms target, so it never under- or over-runs on a device clock a few tenths of
a percent off. The device pulls from the ring on SDL's audio thread.

Every 5 s the runner logs `N frames in X s (F/s), ms p50/p95/p99: frame …, core …, audio …, fence
…, acquire …, record …, present …, sleep …, audio queued …` (`FrameStages`). A `fence` near the frame
time means the GPU is the limit: one frame is in flight, so a preset slower than the core's interval
halves the rate and drains the audio queue. On a Kirin T90A tablet at 2800x1840 with mGBA,
crt-1tap-bloom (26 passes) runs at 33/s (frame p50 29 ms, fence 16-20 ms, audio queued 0), crt-1tap
and None hold 59.7/s (frame 16.7/17.2/17.4 ms, fence 0, audio queued 40-50 ms).

**Shader resolution.** The `shader` stage is the preset's own GPU time (two timestamps around the
chain, read after the next frame's fence). Under Auto, `ShaderScale` steps the chain to 85, 70, 60
or 50 % of the placed rectangle when a 30-frame median passes 90 % of the core's frame interval,
logging `shader resolution down to 70% (GPU 20.0 ms of a 16.7 ms frame)`, and back up one step
after four windows whose median, grown by the area the step adds, stays under 75 %. Below 100 % the
chain draws into `ReducedOutput`'s image of the rectangle's scaled size, its whole viewport, so its
viewport passes shrink with it, and that image is drawn bilinearly into the rectangle; at 100 % the
chain draws into the target as before. librashader sizes viewport passes by the output image, the
whole target, not the rectangle, so at full resolution a letterboxed frame costs as much as a
stretched one. A queue without timestamps keeps Auto at 100 % and says so. To check:
`opensu-retro --hidden` with `OPENSU_WIDTH`/`OPENSU_HEIGHT` as the target size. On an RX 6700 XT,
crt-1tap-bloom at 1280x800 takes 0.5-1.4 ms and Auto never steps; at 16384x4096 Full runs at 31/s
(shader 30.7 ms) and Auto steps to 85, 70 and 60 % within 1.3 s and holds 59.7/s (shader 11.6 ms),
then back up to 70 % once `POST /video` changes Stretch to Core.

**Saves.** `<saves>/<rom stem>.srm`, loaded after the game and before the first frame, checked
once a second and at exit, and written only when the RAM differs from what was last loaded or
written, through `fileio::writeWhole` (a `.part` file and a rename), so a crash mid-write leaves the
previous file. A file smaller than the core's save RAM fills its start, as RetroArch loads it: mGBA
reports 128 KiB (`GBA_SIZE_FLASH1M`) until the game first touches its save chip, then the chip's
size (32 KiB for SRAM), and its save is the start of that buffer. RAM that shrinks to an unchanged
start of what was loaded is not a change, so learning the save type writes nothing. A missing file
starts the game with the core's blank save RAM; a core without save RAM leaves any file alone. A
failed write is logged and tried again at the next check.

LRPS2 keeps no save RAM; its saves are a memory card. The catalog starts it with
`pcsx2_shared_memory_cards=disabled`, under which its `retro_load_game` puts the card folder at the
save directory and names slot 1's card after the content (`<saves>/<rom stem>.ps2`, slot 2 off;
`libretro/ps2` `libretro/main.cpp`, `pcsx2/VMManager.cpp`), so each profile has its own card per
game. With the option on, both cards were `<system>/pcsx2/memcards/Mcd001.ps2` and `Mcd002.ps2`,
one pair for every game and profile. A game without its card starts from a copy of that shared
`Mcd001.ps2` (`retro/lrps2_card`, before the core loads), which is never written; a card is never
written over. The card is not yet one of the game's save slots (`game_saves`), so Copy saves,
backups and sync do not carry it.

Invariant: a save file the runner did not load is never written over. A file larger than the core's
save RAM, or one that cannot be read, is a load failure (`SaveError`): the game is not started, the
runner exits 2 with the reason in the control file for the shell's toast, and the file is left
byte for byte. Refusing beats a backup or a blank start: a blank start writes the game's freshly
formatted save over the player's within a second, and a run whose saves are not kept loses the
session's progress silently. Check it with `opensu-retro --hidden` on a copy of a save padded past
the core's size: exit 2, the error naming the file, the copy's hash unchanged.

**Cores.** A built-in emulator's core is looked for in `<data>/opensu/cores/` (openSU's
downloads), then `~/.config/retroarch/cores`, `/usr/lib64/libretro`, `/usr/lib/libretro` and
`/usr/lib/x86_64-linux-gnu/libretro`. Downloads come from
`https://buildbot.libretro.com/nightly/linux/x86_64/latest/<core>.so.zip` over HTTPS only; the `.so` taken out of the zip is
checked against the CRC-32 in that folder's `.index-extended`, which is the library's, not the zip's; the buildbot publishes no stronger
digest, so this catches corruption, not a compromised host. The listing and the zip are fetched with
every redirect hop on the folder's own origin. The zip must hold exactly `<core>.so`, which is
written whole (`fileio::writeWhole`), then its record `<core>.json` (build date, CRC-32, URL); the
update check compares the listed CRC-32 with the record's. Only a core with a record is openSU's to
update and remove. With the zip, the core's libretro core info `<core>.info` is taken out of the
buildbot's `assets/frontend/info.zip` (same origin) and written beside it; the update check fetches
it for a core downloaded without one. A core installed another way has its info in
`~/.config/retroarch/info` or `/usr/share/libretro/info`.

A core the buildbot's build cannot run comes from openSU's own build: its catalog entry's
`"release"` names a GitHub release and, by build (`coreTarget.build`: `android-arm64-v8a`, ...), the
asset to take; a build it names none for takes the buildbot's. The asset is a zip holding the one
library, downloaded through `EmulatorRelease::fetch` and refused unless it matches a SHA-256: the
one the catalog pins for that build (`"sha256"`, by the same build names as `"assets"`; a release
publishing another is refused), else the one the release publishes. Its info still comes from the
buildbot, its record keeps the release's tag as its date and the asset's URL, which the update check
compares. Settings › Emulators names its source "openSU build"; a buildbot core downloaded before
the catalog named the release stays "libretro buildbot" and is offered as an update. MAME 2010 on
Android (`docs/issues/more-libretro-cores.md`) and mGBA on every platform come this way.

mGBA (built-in) is the `SomeoneIsWorking/mgba` fork, branch `opensu`: libretro/mgba master with
the link-cable multiplayer of libretro/mgba PR #318, a netplay session's clock and the upstream MBC
fix for Final Fantasy Adventure (`docs/issues/runner-core-crashes.md`). The fork's `release.yml`
builds `mgba_libretro` with mGBA's CMake (`-DLIBMGBA_ONLY=ON -DBUILD_LIBRETRO=ON`) for each
`coreTarget.build` on a pushed `20*` tag and uploads `mgba_libretro_<build>.zip` with its
`.sha256` to the tag's release as each build finishes; the catalog pins the tag (`releases/tags/<tag>`) and each zip's SHA-256. Its
`"links": ["gba"]` makes it the core GBA games link on, and only as that release's download
(`InstallSource::ReleasedCore`): a buildbot or RetroArch `mgba_libretro` plays alone. Saves do not
move: a shell launch keeps them by profile and system, not by core.

**BIOS.** A core's BIOS files are its info's `firmware*` entries, looked for in the runner's
system folder (`<data>/opensu/system/`). The catalog's `"withoutBios"` says what the core does
when none is there: `hle` (mGBA, PCSX ReARMed), `open` (melonDS DS's FreeBIOS), `skipped` (boot
ROMs and add-on BIOSes: Gambatte, Snes9x, Mesen), `needed` by default. Settings › Emulators shows,
for each chosen built-in core but the arcade ones, the mode ("HLE built in", "Your BIOS file
<name>", ...) and each file in use, there but not used, or missing. A core that takes images from
the folder by its own rule has `"biosScan"` in the catalog; `pcsx_rearmed` applies PCSX ReARMed's
`loadPSXBios` rule (its own names, then every file; exactly 512 KiB, the reset or "PS compatible"
signature, one per region), so the rows name the file it will load, listed or not; `lrps2`
applies LRPS2's (PCSX2's core): every 4 to 8 MiB file of `pcsx2/bios` whose ROM directory, found
by its `RESET` entry, holds `ROMVER`, its zone from ROMVER's region letter, the first in the
folder's order booted (the default of its `pcsx2_bios` option). Its info lists the folder itself,
which reads in use when it holds such a file. Check it in the product: Settings ›
Emulators, the "<System> BIOS" rows; the runner's log line for the core's choice (PCSX ReARMed:
`found US BIOS file` or `HLE BIOS initialized.`; LRPS2: `Bios Found: USA v01.60(...)`). Built-in entries come after the standalone ones in the catalog, so a system's
automatic emulator stays the standalone one where it is installed.

**System files.** A core that reads data from the system folder besides BIOS files names it in the
catalog: `"systemFiles": {url, sha256, folder, into}`, one folder of an HTTPS zip pinned by SHA-256,
unpacked under `<system>/<into>` (`core_system_files`). It is the core's own source tree at the
revision the buildbot builds it from, so the data matches the code that reads it: Dolphin's is
`libretro/dolphin`'s `Data/Sys` at the commit its library names (`strings` on the `.so` shows it
after `GBPlayer: ROM save set to`; GitHub's archive of a commit is byte-stable), into
`dolphin-emu/Sys`, the folder the core logs as `System Directory set to`. When the buildbot's core
moves to a newer commit the pin stays until the catalog moves it. `EmulatorDownloads` installs it
after the core (Settings › Emulators, the details page), and the update check installs it for a
core found without it; every redirect hop stays on the zip's origin. The record
`<cores>/<core>.system.json` lists the pin and each file openSU wrote. Only those are replaced or
removed: a file already there that openSU did not write (a player's `Sys/GC/USA/IPL.bin`) stays,
a file of an older pin the new one lacks goes with its empty folders, and the files are claimed in
the record before they are written, so an install cut short is finished by the next. Removing the
core removes them. Without Dolphin's `Sys` the core logs `Core file codehandler.bin missing!`,
`Post-processing shader not found: .../Sys/Shaders/default_pre_post_process.glsl` and, on Wii,
`ScanDirectoryTree error on .../Sys/Wii`; with it none of these appear. PPSSPP's `PPSSPP/` assets
(`hrydgard/ppsspp`'s `assets/` at its core's revision) can go through the same field; not pinned
yet.

**Romsets.** An arcade core's catalog entry names its listing: `"romsets": {"files": [...]}`
(HTTPS datafiles merged in order) or `"romsets": {"release": {api, asset, downloads}}` (the latest
release's zip holding one `.xml`, size and SHA-256 checked). `RomsetDats::refresh` fetches a
listing that is missing or older than 30 days, every redirect hop on the file's origin, and keeps
the old one when the fetch fails; a file in an older line format counts as none. A library scan
never waits on a fetch: `RomsetMatcher` asks
`RomsetLists` for each due listing and matches with what is kept; `RomsetLists` fetches them in
turn on its own thread, installed cores' first (`arcadeCoresOf`), a failed one again no sooner than an hour later, and keeps every fetch's
state for the session. While a core's listing downloads and none is kept its verdict is `Pending`;
with no match yet and a verdict pending the game's romset reads "Checking romset…", it cannot start
(`unavailable` says the lists are downloading) and a system's emulator pick waits. Each fetch is a
row of the Downloads page ("Romset list · <core>", bytes, then Finished, or Failed with the reason
and the retry time) and its end is toasted; the shell then reads the catalog again, which gives the
verdicts. A pending verdict is never kept in `matches.json`. A zip's set is
its stem, lower case. A set matches a core when every ROM it needs (a `nodump`, optional or
CRC-less ROM is not needed; of BIOS-choice ROMs only the default choice's; in a listing with no
`biosset`, as FinalBurn's are, any one of the `isbios` set's ROMs stands for them all, which
`RomsetDat::markChoices` marks) is found by CRC-32 and size in the zip, its `romof` chain's zips or its devices' zips (a device's own devices included)
in the same folder, and each disk is at `<set>/<disk>.chd`. A zip any listing marks `isbios` is
not listed as a game (`Provider::list`), but stays a parent to the games beside it. `applyRomsets` keeps the search's
emulator when it matches, else takes the first matching one installed, else names the first
matching one as not installed (the details page offers its download); with every core checked and
none matching the game's `launch` is empty and `unavailable` is "No arcade core matches this
romset", with `romsetNearest` the core that lacks the fewest files and which. A core whose listing
cannot be had is unchecked and changes nothing. `library::applyEmulatorPicks` skips a system's pick
the romset does not match (the game layer's automatic value); a game's own pick always applies. Verdicts are kept in
`<cache>/romsets/matches.json` with the stamp (size, modification time, as the folder's listing
gave them) of the zip and of every file the match looked at by its name in the folder, absent ones
included, and the listings' source CRC-32s; a scan reads nothing more of a zip whose stamps all
hold. A zip is read through `RomTree::open`, on Android a provider's descriptor that must seek. A
verdict names the zips it held; in a picked folder `addBesideZips` adds each to the launch of a
matching core as `--beside <uri>`, and `RetroActivity` copies it beside the game and takes the
argument out, for cores open them by path. Disks are not copied.

**Rebuilt romsets.** A set no plain match gives a core may still convert: `convertSet` looks its
name up through `RomsetConversions::aliasesOf` and asks `plan` for each ROM the listing wants, by
CRC-32 from the zips held, else by a recipe keyed on the target's CRC-32, else a CPS2 `.key` from
`assets/cps2_keys.json`. A complete plan is `RomsetFit::Converts`, which starts a game as a match
does and reads "through conversion". Both rank alike, in catalog order, so a stock core that gives
real button names comes before MAME 2010. The launch carries a
`RomConversion` (target, source zips, stamp). `GameLaunch` builds it on its own thread in the shell,
which can read Android documents, and replaces the zip in the arguments. The rebuilt zip is complete,
so no `--beside` is added and `GameStaging` passes its path through. `RomsetConverter` writes
`<target>.part`, checks every ROM's CRC-32 against the listing and renames it; bytes unlike the
listing fail the launch. A recipe never forges a CRC: FinalBurn Neo finds ROMs by CRC-32 and size,
treats a wrong one as missing, and fails a zip whose directory lies. The zip must take the driver's
name, and its folder must not be named after a FinalBurn subsystem (`nes`, `gba`, ...), so it sits
under the stamp.

`tools/romset_conversions.py` generates the recipes without ROM data: mirrors, joins, splits and pads
are proven by CRC arithmetic from the listings alone. Interleaves cannot be, so the CPS3 ones were
proven on real bytes. FB Alpha 2012's key table names sets differently from FinalBurn Neo (`19xx`
there is `19xxu`), so keys go by the `.key`'s CRC. Removing `assets/cps2_keys.json` only stops keys
being written.

**Rotation.** `SET_ROTATION` is honoured for a software core: `VideoFrame::take` turns the pixels, so
everything after it sees an upright frame. A core's aspect is the displayed one and is not inverted.
FB Alpha 2012, FinalBurn Neo, MAME 2003-Plus, MAME 2010 and MAME all give it that way. With a
hardware context the command is refused.

**State size.** `Core::unserialize` asks the size first: FB Alpha 2012 refuses a state until it has
been asked, which a resume in a fresh run would otherwise hit.

## 5. Phases

1. Software cores: mGBA (GB, GBC, GBA), then Gambatte, snes9x, Mesen, PCSX ReARMed, melonDS DS
   (NDS; its software renderer by default, FreeBIOS and built-in firmware when no files are
   given), and for Arcade FinalBurn Neo, FB Alpha 2012, MAME 2003-Plus, MAME 2010 and MAME, picked per game by
   romset. Genesis Plus GX and PicoDrive wait for Mega Drive, Master System, Game Gear, Sega CD
   and 32X systems in `rom_systems`. Runner, input, audio, save RAM, core downloads through
   Settings › Emulators and the details page.
2. Built: save states, reset and Save and close with its resume from the Guide menu over a
   loopback `lucent::http::Server` in the runner; a crash named in the shell's toast, a load
   failure told by its reason.
3. Built: GPU cores (`RETRO_ENVIRONMENT_SET_HW_RENDER` with `RETRO_HW_CONTEXT_VULKAN` on the
   presenter's negotiated device, **GPU cores** in §4): Mupen64Plus-Next and ParaLLEl N64
   (ParaLLEl-RDP and ParaLLEl-RSP), Beetle PSX HW and SwanStation (their Vulkan renderers),
   PPSSPP (PSP) and Flycast (Dreamcast).

## 6. Testing

- `tests/retro/` builds three fake cores as shared libraries from one source: one draws a known
  pattern, plays a tone, reads the joypad, writes save RAM and serializes its save RAM and frame
  counter (reset zeroes the count); one crashes in `retro_run`; one cannot serialize.
- Unit tests through the shipping code: pixel conversion, resampler fill control, joypad mapping
  and sticks (the furthest of two pads, a pad leaving),
  save RAM round trip and atomic write, a smaller save into the RAM's start and a larger or
  unreadable one refused untouched, environment answers (an OpenGL context refused by name, a
  Vulkan one taken, Vulkan preferred).
- `netplay_link_test`: the session file and what it refuses; `tests/netplay` the session itself.
- `arcade_panel_test`: panels from the tables FinalBurn Neo, FB Alpha 2012 and MAME 2010 send, the
  rows by count, every control beside the picture on a wide screen, several fingers at once and the
  stick's ways through `TouchLayer`.
- `core_test` loads the fake core through `Core` with the real owners: frames, samples, the
  joypad and save RAM reach them, a second core and a missing library or ROM are `CoreError`s.
- `runner_test` runs `opensu-retro --hidden --validate --frames 30 --frame-png` against the fake
  core (RGB565 bars letterboxed in the PNG, the `.srm` holding the frame count, a second run
  continuing from it), exit 2 for a missing core or ROM, exit 1 for a bad command line, and the
  crashing core killing the runner by SIGSEGV with the last save intact. With `OPENSU_RETRO_CONTROL` set: the control file
  appears with the runner's pid and port, `/status` names the game, loading before a save is 404, a
  save writes the `.state`, a later load leaves the `.srm`'s frame count showing the frames between
  them undone, a reset zeroes it, the stateless core's save is 409 with its reason, an unknown
  route 404, `/input` holding Start reaches the core's `.srm` and Guide or a stick past 1 is 400,
  `/touch` with a stick and a button held at once reaches it too,
  SIGTERM exits 0 and removes the file, and a missing ROM exits 2 leaving the error. A save smaller
  than the core's RAM continues the frame count; a larger one exits 2, named in the control file,
  and is left as it was.
- `retro_session_test` and `quick_menu_controller_test` run `RetroSession` against a fake runner
  (`tests/support/fake_runner.hpp`): the per-launch file, the port read once announced, reasons
  passed through, the load failure handed over once; the rows, the disabled Load state, the two
  presses and a moved focus taking the first back. `handoff_test` launches a scope process that
  kills itself with SIGSEGV (reported 139), one that exits 0 (0) and one force-closed (not
  reported); `game_end_test` pins the toast wording.
- The tests need a display for the hidden SDL window and any Vulkan driver: the CI workflow wraps
  `tools.check ci` in `xvfb-run`, the check container gets that display forwarded in
  (`tools/podman.display_words`), and the image has lavapipe and the validation layers.
- `romset_match_test` matches zips built in its scratch folder against a datafile and a
  `-listxml` written in the test: parent, renamed member, BIOS choice, device of a device, disk,
  kept verdicts and what invalidates them, the pick and the no-match text; and `RomsetLists`
  against a loopback host: two files' bytes counted together, a release zip, a 404 with its retry
  time, and a scan that gets `Pending` from a listing held back by the host instead of waiting.
- `romset_convert_test` builds an old zip with `zip::Writer` and converts it against a written
  listing: alias, held file, halved mirror, interleaved lanes, the key, the kept zip, a lacking
  recipe, bytes unlike the listing refused, and the converting core picked.
- A real core and ROM are checked by hand on this machine, never in CI (no ROMs in CI).
