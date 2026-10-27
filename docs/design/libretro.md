# Built-in libretro runner

Status: phases 1, 2 and 3 built (`opensu-retro` with its control channel and Vulkan GPU cores,
S032); core downloads and launch are S031.

## 1. Goal

Retro systems play inside openSU with one fixed controller mapping in the runner for every core
(a player's button mapping, S033, is applied by the shell to the virtual pad the runner reads),
saves that survive any crash, and the Guide overlay and quick menu working as they do over any
game. A core that crashes ends its game, never the shell.

Non-goals for now: rewind, run-ahead, netplay, achievements, cores that draw with OpenGL, GLES or
Direct3D (refused by name; the runner has no GL path), anything a standalone emulator already does
better (Switch, PS2 and later, Wii U, GameCube/Wii, Xbox, Xbox 360).

## 2. Shape

Each game runs `opensu-retro`, a second executable built from this repository, in the game's own
systemd scope like any other emulator. It loads one core with `dlopen`, opens its own window and
runs the core's frame loop. Gamescope shows that window, the shell's overlay goes over it, and the
handoff sees it appear and end exactly as it sees a standalone emulator.

```
shell ── Handoff (scope) ──▶ opensu-retro --core <so> [options] <rom>
                                 │ SDL window, Vulkan    │ evdev pads (the shell's virtual pad
                                 │ SDL audio             │ while the shell holds the real ones)
                                 └ saves under <data>/opensu/saves/<system>/
```

Why a child window and not frames sent back to the shell: the overlay, scopes, game-quit handling
and orphan cleanup already exist for game windows; a GPU core draws with the child's own Vulkan
device; there is no frame transport to build or to add latency.

The runner does not use the shell's renderer: SDL3 gives it the window, keyboard and sound device, and it draws
with Vulkan directly, so a core using libretro's Vulkan hardware interface
(`retro_hw_render_interface_vulkan`) is handed the runner's instance, device and queue.

One core per process is also what libretro's callbacks assume: they carry no context pointer, so
the runner's callback trampolines reach the one loaded core through a single pointer in
`retro/core.cpp`.

## 3. Owners

| Owner | Owns |
| --- | --- |
| `cmake/Libretro.cmake` | `libretro.h` and `libretro_vulkan.h` (MIT) from libretro-common at a pinned commit, SHA-256 checked |
| `cmake/Librashader.cmake` | librashader from the `SomeoneIsWorking/librashader` fork at a pinned commit: its header (MIT, SHA-256 checked), and `librashader-capi` built with cargo (Rust 1.88 or newer) into `<libdir>/opensu/librashader.so`, staged in the build tree and installed |
| `src/retro/core.*` | One loaded core: `dlopen`, the `retro_*` symbols, init/load/run/unload in order, the callback trampolines |
| `src/retro/core_environment.*` | The environment commands answered: directories, pixel format, core options, log, input descriptors, geometry and timing changes, the hardware-render commands (through its `HardwareRender`); anything else is refused |
| `src/retro/core_options.*` | The core's option definitions (v0 variables, v1, v2) and their current values: the defaults, except a value the launch chose (`--option`) that the core offers for that key |
| `src/retro/hardware_render.*` | A GPU core: `SET_HW_RENDER` (Vulkan taken, any other context refused with its name kept as the load failure's reason), `GET_PREFERRED_HW_RENDER` (Vulkan), the context negotiation interface kept for the device, `GET_HW_RENDER_INTERFACE` (`retro_hw_render_interface_vulkan` on the presenter's instance, device and queue), `set_image`, semaphores, command buffers, the signal semaphore, the queue lock, the sync index; `context_reset` once the presenter is up, `context_destroy` before the game is unloaded; the image of each `RETRO_HW_FRAME_BUFFER_VALID` frame, kept for duplicated frames |
| `src/retro/video_frame.*` | A frame in the core's pixel format (0RGB1555, XRGB8888, RGB565) to RGBA, duplicate frames kept |
| `src/retro/audio_sink.*` | The core's samples into a ring, resampled with dynamic rate control so the buffer stays near its target |
| `src/retro/joypad.*` | The pads' controls (`gamepad::Button`) and the keyboard to `RETRO_DEVICE_JOYPAD` state, one fixed mapping; the pads' sticks (`gamepad::Sticks`) to `RETRO_DEVICE_ANALOG` |
| `src/retro/save_memory.*` | The core's save RAM: loaded before the first frame, written atomically when it changes (checked once a second) and at exit |
| `src/retro/audio_output.*` | Where the sink's frames go: the default playback device (SDL audio stream, pulled on SDL's thread), or, hidden, drained by the clock at the output rate with nothing played |
| `src/retro/presenter.*` | The only owner of the SDL window and Vulkan presentation: window events, swapchain (recreated on resize or out-of-date), each frame's command buffer, submission (a GPU core's semaphores waited, its command buffers first, its signal semaphore signalled) and present; hidden, an offscreen image instead of a swapchain, read back for `--frame-png`; the wait on the last frame a core's `wait_sync_index` is |
| `src/retro/frame_compositor.*` | What is drawn into a target (`change` swaps the video settings between frames, the new preset built before the old one goes): the render pass the targets' framebuffers are made for, the frame texture uploaded each fresh frame or a GPU core's own image sampled in its layout (queue family ownership taken and given back when it names another family, then a barrier so the core's next writes wait for the read), one pipeline drawing it where `placeFrame` puts it for the video settings (aspect, integer scale, overscale clipped to the target) with the chosen sampling and filter, black around it; with a shader preset its chain draws into that rectangle instead, from the texture or the core's image |
| `src/retro/frame_filter.*` | The fragment shader for a sampling and filter, the built-in filters' parameters (`scanlineGrid`, `lcdGrid`, `crtLottes`) and the push block each draw hands the shader |
| `src/retro/librashader_library.*` | librashader opened by its path (`Config::librashader`), ABI and API checked, its calls with their errors as `ShaderError`; a missing file refuses the preset |
| `src/retro/shader_chain.*` | One slang preset built on the runner's device (render passes, one frame in flight) and recorded from the frame image into the placed rectangle; history (cleared on the first frame), feedback and LUTs are librashader's |
| `src/retro/vulkan_device.*` | The Vulkan instance (validation layer and its error count on request) and the one device and graphics queue, made through a GPU core's context negotiation when it has one (its `get_application_info`; `create_instance`, `create_device2` or `create_device` with the runner's extensions and layers added), `destroy_device` before the instance goes; the queue's lock, held for every submit, present and device wait |
| `src/retro/png_encoder.*` | RGBA to PNG bytes through libpng |
| `src/retro/shaders/frame.{vert,frag}`, `sharp_bilinear.frag`, `frame_grid.glsl`, `crt_lottes.frag` | The frame's shaders (sharp-bilinear: integer prescale then bilinear; `frame_grid.glsl`: the scanline and LCD-grid bands both sampling shaders apply; crt-lottes), compiled by glslc at build time into SPIR-V headers under the build tree |
| `src/retro/save_state.*` | The one state slot `<saves>/<rom stem>.state`: `retro_serialize` written whole, read back through `retro_unserialize` |
| `src/retro/control_server.*` | The runner's loopback `lucent::http::Server`: the control file (port and pid, or the load failure), the routes, the hand-over of each command and of the control pad's input to the frame loop |
| `src/retro/runner_app.*` | Composition: core and owners, presenter, pads, keyboard, sound, the loop and its pacing, save checks, control commands between frames, the hidden mode for tests |
| `src/retro/main.cpp` | `opensu-retro`'s command line, the `--settings` file read into `settings::VideoSettings` |
| `src/settings/setting_layers.*` | The setting layers (Global, System, Emulator — the core layer —, Game) and the one resolution rule, each value with the layer it came from |
| `src/settings/video_settings.*` | `video.aspect`, `video.integer`, `video.sampling`, `video.filter`, `video.shader` (a path in the pack, `""` for None; a preset replaces sampling and filter): their choices and labels, resolution to typed values, the per-launch settings file and `POST /video`'s body (`videoJson`, `parseVideoJson`) |
| `src/settings/game_settings_store.*` | `<config>/opensu/game-settings.json`, every layer of every key |
| `src/settings/emulator_pick_migration.*` | The one-time move of `settings.json`'s `emulators` and `systemEmulators` into the game settings |
| `src/app/game_settings.*` | The layers as the shell holds them, saved with a toast on failure; the words a source reads with |
| `src/app/retro_launch.*` | A launch on a built-in core: its video settings resolved, written to the launch's settings file, `--settings` added; the core options the layers keep as `--option` |
| `src/settings/setting_schema.*`, `setting_scope.*`, `core_option_settings.*` | Every layered setting declared once (video, shader preset, frame generation, the emulator pick, a core's options), validation and clear from it; a scope's rows with the value in effect and its source |
| `src/app/layer_settings.*` | Settings › Emulators' Global, Consoles and Built-in cores rows, each opening the layered editor |
| `src/app/layered_settings.*`, `layered_editor.*` | The one layered editor (any scope; a page or a panel over the game) and what it edits, each change saved and applied to the running game |
| `src/app/core_option_store.*` | Each core's option definitions as last seen, for editing while nothing runs |
| `src/app/shader_preset_tree.*` | The shader pack's folder tree and a preset's label; the picker is `ShaderPresetScreen` in `layered_editor` |
| `src/library/shader_pack.*` | libretro's `shaders_slang.zip`: the latest build (HEAD: `Last-Modified` and size), the download checked by length and each member's CRC-32, unpacked into `<data>/opensu/shaders/slang/` with its record, removal, the presets it holds |
| `src/library/libretro_cores.*` | Finding a core and its info file; core downloads from the libretro buildbot under `<data>/opensu/cores/`, with their info and record |
| `src/library/core_bios.*` | A core's BIOS files from its core info, what it does without them, which are in the system folder |
| `src/library/romset_dat.*` | An arcade core's ROM-set listing: a Logiqx datafile or MAME `-listxml` parsed, its line format under the cache |
| `src/library/romset_dats.*` | The arcade cores' listings fetched over HTTPS from the catalog's `"romsets"` and kept under `<cache>/romsets/` for 30 days |
| `src/library/romset_lists.*` | Those fetches off the library scan, one at a time on a thread, each one's state (bytes, done, failed and when it is tried again) for the Downloads page |
| `src/library/romset_match.*` | An arcade zip's central directory against each listing, the verdicts kept by file stamps, the game's emulator picked by them |
| `src/library/emulator_choice.*` | The `emulator` key at the Game and System layers: game pick, romset, system pick, search; the layer keys a game reads its settings through |
| `assets/emulators.json`, `src/library/emulator_catalog.*` | A built-in emulator is an entry with `"core": "<name>_libretro"` and no launchers or release; `"coreOptions"` the core options it starts with (a GPU core's Vulkan renderer) |
| `src/library/emulators.*` | A built-in emulator is installed when its core is found and `opensu-retro` is beside `opensu` (`config::retroRunnerBeside`); it starts as `opensu-retro --core <core> [--option <key>=<value>]... <rom>`, with `--hidden` from a hidden shell |
| `src/app/emulator_downloads.*`, `emulator_settings.*`, `details_controller.*` | Download, update check, update and removal of a core and of the shader pack, on the same paths as an AppImage; the Shader presets rows; the details page's video sources, Game settings button and Clear |
| `src/app/retro_session.*` | The shell's side of the control channel: each launch's control file, the port read from it, the requests (`setVideo` for `POST /video`), the load failure handed over and the file removed |
| `src/app/quick_menu_controller.*` | Game settings, Save state, Load state and Reset game while the running game answers on a control channel |
| `src/app/game_end.*`, `src/launch/handoff.*` | How a game ended (`Hooks::ended`, the scope's exit status) and the toast it gets |

`retro` depends on `gamepad`, `fileio`, Lucent, nlohmann/json, SDL3, the Vulkan loader and libpng;
`config` is read by `main.cpp` only. Nothing outside `retro` and the `opensu-retro` executable links
it. Only `presenter`, `frame_compositor`, `shader_chain`, `librashader_library`, `vulkan_device`
and `hardware_render` (and through it `core_environment`) include Vulkan, only the last two
`libretro_vulkan.h`, and only `presenter` uses SDL video; `audio_output` includes SDL audio;
`joypad` uses SDL's scancode values only.

## 4. Contracts

**Command line.** `opensu-retro --core <path.so> [--saves <dir>] [--system <dir>] [--settings
<file>] [--option <key>=<value>]... [--hidden] [--validate] [--frames N] [--frame-png <file>]
<rom>`. `--option` sets a core option for this run: the core's value for that key when it offers
it, else (a warning logged) its default. `--settings` names a JSON
object of resolved video settings (`{"video.aspect": "4:3", "video.integer": "off",
"video.sampling": "nearest", "video.filter": "none", "video.shader": "<absolute .slangp path>"}`);
a key it lacks keeps its default, a value that is no choice is exit 1. The runner never reads `game-settings.json`. `--saves` defaults to
`<data>/opensu/saves/<core name>/` (the core file's stem without `_libretro`; the launcher passes
the system's folder), `--system` to `<data>/opensu/system/`, both from `config::Config`. `--hidden`,
`--validate`, `--frames` and `--frame-png` are for tests and tools: `--hidden` is an unmapped SDL
window (`SDL_WINDOW_HIDDEN`), drawing into an offscreen image, no sound device (the sink is drained
by the clock, so rate control still runs), no pads and no keyboard; `--validate` enables
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
A death by any signal but SIGTERM, SIGKILL, SIGINT and SIGHUP is toasted as `<title> crashed
(SIGSEGV)`; a load failure the runner left in its control file is toasted as its reason instead.
Other ends keep the launch's own toast (`<title> closed`, or its failure).

**Control channel.** The shell sets `OPENSU_RETRO_CONTROL=<runtime>/opensu/retro-<shell pid>-<n>.json`
(`<runtime>` is `$XDG_RUNTIME_DIR`) in every launch's environment; `config::Config::retroControl` is
how the runner reads it. With it set, once the game has loaded and the window is up, the runner
listens on a free loopback port (`lucent::http::Server`, port 0) and writes
`{"port": N, "pid": P}` to that file atomically (`fileio::writeWhole`); it removes the file when it
exits. When the core or game cannot be loaded (exit 2), or the presenter or its shader preset
cannot be set up (exit 1), it writes `{"error": "<reason>"}` there instead and leaves it; the shell reads it once the launch is over and deletes it. Routes:

| Route | Answer |
| --- | --- |
| `GET /status` | `{"core", "rom", "frame", "hasState"}`: the core's name, the ROM path, frames run, whether the slot holds a state |
| `POST /state/save` | 200; 409 with the reason when the core cannot serialize (`retro_serialize_size` 0 or `retro_serialize` false); 500 when the slot cannot be written |
| `POST /state/load` | 200; 404 when no state is saved; 409 when the core refuses it |
| `POST /reset` | 200 after `retro_reset` |
| `POST /video` | 200 once the video settings in the body (`settings::videoJson`, the `--settings` file's format) are drawn from the next frame; 400 for a body that does not parse; 409 with the reason when the shader preset cannot be built, the settings drawn before kept |
| `GET /frame.png` | The last frame shown as PNG, letterboxing included (hidden runs only; 409 otherwise) |
| `POST /input` | 200: the control pad holds `{"buttons": [names], "left": [x, y], "right": [x, y]}` (each part optional, sticks from -1 to 1, y down) until the next post, `{}` letting go; 400 for anything else or Guide |

Every route but `GET /status` and `POST /input` is queued for the frame loop, which runs it between frames, never beside `retro_run`;
the request waits for the answer, and is answered 503 when the loop has not taken it within 3 s or
the runner is closing. Error bodies are the reason as plain text. The slot is
`<saves>/<rom stem>.state`, written whole, so a crash mid-write keeps the previous state. The shell
reads the port lazily and talks through `lucent::http::get_loopback` and `post_loopback`; requests
run on the shell's loop, which a runner that stops answering holds for up to the client's 5 s
socket timeout.

**Quick menu.** While the running game answers `/status`, the quick menu (Guide + A) shows Save
state (one press), Load state (a second press confirms; an Info row reading "No state saved" when
the slot is empty) and Reset game (a second press confirms) above Close game. A focus move takes the
first press back. The results are toasts: "State saved", "State loaded", "Game reset", or the
runner's reason as an error. Its "Game settings" row opens the layered editor (below) as a panel over the
game; B from it comes back to the quick menu on that row.

**Input.** The runner reads every evdev pad without grabbing it. While the shell holds the real
pads during a game, the only pad that moves is the shell's virtual Xbox 360 pad, so the Guide
chords stay the shell's and every core sees the same layout. Run alone, it reads the real pads
through the same translator (`gamepad::Pads` with `OwnVirtualPads::Read`, never holding). Pad
positions follow the Xbox layout: A (bottom) is libretro's B, B is A, X (left) is Y, Y is X; L1/R1
are L/R, L2/R2, L3/R3, Select, Start and the d-pad and left stick are themselves; Guide reaches no
core. Keyboard: arrows, Z (B), X (A), A (Y), S (X), Q/W (L/R), Enter (Start), Right Shift (Select);
Escape quits. Port 0 only. An analogue trigger is L2 or R2 past half its travel
(`gamepad::triggerPressAt`). Both sticks are also `RETRO_DEVICE_ANALOG` (`gamepad::Pads::sticks`,
on the virtual pad's -32768 to 32767, y down; of several pads, each axis the one furthest out);
`RETRO_DEVICE_INDEX_ANALOG_BUTTON` reads 0. The control channel's pad (`POST /input`) is one more
source beside the pads. There is no mouse, so `RETRO_DEVICE_POINTER` reads 0: melonDS DS's touch
screen is its joystick mode, the right stick moving its cursor and R2 touching.

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
submission. The core's frame goes through the same `placeFrame`, sampling and filter as a
software frame, or the same shader preset: an image in `SHADER_READ_ONLY_OPTIMAL` is the chain's
input as it is (librashader copies it into the history, which the `TRANSFER_SRC` usage libretro
requires of `set_image` images allows, and gives it back in that layout); one in `GENERAL`, which
the runner may not move, is blitted into the frame texture first and the chain reads that. Every submit, present and device wait holds the queue lock the core's
`lock_queue` takes. At exit: the last frame read back, then `context_destroy` (GPU idle), then
`retro_unload_game`, then the device (`destroy_device`, `vkDestroyDevice`) and the instance,
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

**Saves.** `<saves>/<rom stem>.srm`, loaded after the game and before the first frame, checked
once a second and at exit, and written only when the RAM differs from what was last loaded or
written, through `fileio::writeWhole` (a `.part` file and a rename), so a crash mid-write leaves the
previous file. A missing or wrong-sized file starts the game with the core's blank save RAM and is
kept until the core writes its own. A failed write is logged and tried again at the next check.

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

**BIOS.** A core's BIOS files are its info's `firmware*` entries, looked for in the runner's
system folder (`<data>/opensu/system/`). The catalog's `"withoutBios"` says what the core does
when none is there: `hle` (mGBA, PCSX ReARMed), `open` (melonDS DS's FreeBIOS), `skipped` (boot
ROMs and add-on BIOSes: Gambatte, Snes9x, Mesen), `needed` by default. Settings › Emulators shows,
for each chosen built-in core but the arcade ones, the mode ("HLE built in", "Your BIOS file
<name>", ...) and each file in use, there but not used, or missing. A core that takes images from
the folder by its own rule has `"biosScan"` in the catalog; `pcsx_rearmed` applies PCSX ReARMed's
`loadPSXBios` rule (its own names, then every file; exactly 512 KiB, the reset or "PS compatible"
signature, one per region), so the rows name the file it will load, listed or not. Check it in the product: Settings ›
Emulators, the "<System> BIOS" rows; the runner's log line for the core's choice (PCSX ReARMed:
`found US BIOS file` or `HLE BIOS initialized.`). Built-in entries come after the standalone ones in the catalog, so a system's
automatic emulator stays the standalone one where it is installed.

**Romsets.** An arcade core's catalog entry names its listing: `"romsets": {"files": [...]}`
(HTTPS datafiles merged in order) or `"romsets": {"release": {api, asset, downloads}}` (the latest
release's zip holding one `.xml`, size and SHA-256 checked). `RomsetDats::refresh` fetches a
listing that is missing or older than 30 days, every redirect hop on the file's origin, and keeps
the old one when the fetch fails. A library scan never waits on a fetch: `RomsetMatcher` asks
`RomsetLists` for each due listing and matches with what is kept; `RomsetLists` fetches them in
turn on its own thread, a failed one again no sooner than an hour later, and keeps every fetch's
state for the session. While a core's listing downloads and none is kept its verdict is `Pending`;
with no match yet and a verdict pending the game's romset reads "Checking romset…", it cannot start
(`unavailable` says the lists are downloading) and a system's emulator pick waits. Each fetch is a
row of the Downloads page ("Romset list · <core>", bytes, then Finished, or Failed with the reason
and the retry time) and its end is toasted; the shell then reads the catalog again, which gives the
verdicts. A pending verdict is never kept in `matches.json`. A zip's set is
its stem, lower case. A set matches a core when every ROM it needs (a `nodump`, optional or
CRC-less ROM is not needed; of BIOS-choice ROMs only the default choice's) is found by CRC-32 and
size in the zip, its `romof` chain's zips or its devices' zips (a device's own devices included)
in the same folder, and each disk is at `<set>/<disk>.chd`. `applyRomsets` keeps the search's
emulator when it matches, else takes the first matching one installed, else names the first
matching one as not installed (the details page offers its download); with every core checked and
none matching the game's `launch` is empty and `unavailable` is "No arcade core matches this
romset", with `romsetNearest` the core that lacks the fewest files and which. A core whose listing
cannot be had is unchecked and changes nothing. `library::applyEmulatorPicks` skips a system's pick
the romset does not match (the game layer's automatic value); a game's own pick always applies. Verdicts are kept in
`<cache>/romsets/matches.json` with the stamp (size, modification time) of the zip and of every
file the match looked at, absent ones included, and the listings' source CRC-32s; a scan reads
nothing more of a zip whose stamps all hold.

## 5. Phases

1. Software cores: mGBA (GB, GBC, GBA), then Gambatte, snes9x, Mesen, PCSX ReARMed, melonDS DS
   (NDS; its software renderer by default, FreeBIOS and built-in firmware when no files are
   given), and for Arcade FinalBurn Neo, MAME 2003-Plus, MAME 2010 and MAME, picked per game by
   romset. Genesis Plus GX and PicoDrive wait for Mega Drive, Master System, Game Gear, Sega CD
   and 32X systems in `rom_systems`. Runner, input, audio, save RAM, core downloads through
   Settings › Emulators and the details page.
2. Built: save states and reset from the quick menu over a loopback `lucent::http::Server` in the
   runner; a crash named in the shell's toast, a load failure told by its reason.
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
  save RAM round trip and atomic write, environment answers (an OpenGL context refused by name, a
  Vulkan one taken, Vulkan preferred).
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
  SIGTERM exits 0 and removes the file, and a missing ROM exits 2 leaving the error.
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
- A real core and ROM are checked by hand on this machine, never in CI (no ROMs in CI).
