# Built-in libretro runner

Status: phases 1 and 2 built (`opensu-retro` with its control channel, S032); core downloads and
launch are S031.

## 1. Goal

Retro systems play inside openSU with one fixed controller mapping in the runner for every core
(a player's button mapping, S033, is applied by the shell to the virtual pad the runner reads),
saves that survive any crash, and the Guide overlay and quick menu working as they do over any
game. A core that crashes ends its game, never the shell.

Non-goals for now: shaders, rewind, run-ahead, netplay, achievements, cores that draw with the GPU
(phase 2), anything a standalone emulator already does better (Switch, PS2 and later, Wii U,
GameCube/Wii, Xbox, Xbox 360).

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
and orphan cleanup already exist for game windows; a GPU core in phase 3 draws with the child's own
Vulkan device; there is no frame transport to build or to add latency.

The runner does not use the shell's renderer: SDL3 gives it the window, keyboard and sound device, and it draws
with Vulkan directly, so a core using libretro's Vulkan hardware interface
(`retro_hw_render_interface_vulkan`) can later be handed the runner's instance, device and queue.

One core per process is also what libretro's callbacks assume: they carry no context pointer, so
the runner's callback trampolines reach the one loaded core through a single pointer in
`retro/core.cpp`.

## 3. Owners

| Owner | Owns |
| --- | --- |
| `cmake/Libretro.cmake` | `libretro.h` (MIT) from libretro-common at a pinned commit, SHA-256 checked |
| `src/retro/core.*` | One loaded core: `dlopen`, the `retro_*` symbols, init/load/run/unload in order, the callback trampolines |
| `src/retro/core_environment.*` | The environment commands answered: directories, pixel format, core options (defaults), log, input descriptors, geometry and timing changes; anything else is refused |
| `src/retro/core_options.*` | The core's option definitions (v0 variables, v1, v2) and their current values (the defaults in phase 1) |
| `src/retro/video_frame.*` | A frame in the core's pixel format (0RGB1555, XRGB8888, RGB565) to RGBA, duplicate frames kept |
| `src/retro/audio_sink.*` | The core's samples into a ring, resampled with dynamic rate control so the buffer stays near its target |
| `src/retro/joypad.*` | The pads' controls (`gamepad::Button`) and the keyboard to `RETRO_DEVICE_JOYPAD` state, one fixed mapping |
| `src/retro/save_memory.*` | The core's save RAM: loaded before the first frame, written atomically when it changes (checked once a second) and at exit |
| `src/retro/audio_output.*` | Where the sink's frames go: the default playback device (SDL audio stream, pulled on SDL's thread), or, hidden, drained by the clock at the output rate with nothing played |
| `src/retro/presenter.*` | The only owner of the SDL window and Vulkan presentation: window events, swapchain (recreated on resize or out-of-date), the frame texture uploaded each fresh frame, one pipeline drawing it with its aspect kept and letterboxed black; hidden, an offscreen image instead of a swapchain, read back for `--frame-png` |
| `src/retro/vulkan_device.*` | The Vulkan instance (validation layer and its error count on request) and the one device and graphics queue, the objects a Vulkan core will be handed in phase 3 |
| `src/retro/png_encoder.*` | RGBA to PNG bytes through libpng |
| `src/retro/shaders/frame.{vert,frag}` | The frame's two shaders, compiled by glslc at build time into SPIR-V headers under the build tree |
| `src/retro/save_state.*` | The one state slot `<saves>/<rom stem>.state`: `retro_serialize` written whole, read back through `retro_unserialize` |
| `src/retro/control_server.*` | The runner's loopback `lucent::http::Server`: the control file (port and pid, or the load failure), the routes, the hand-over of each command to the frame loop |
| `src/retro/runner_app.*` | Composition: core and owners, presenter, pads, keyboard, sound, the loop and its pacing, save checks, control commands between frames, the hidden mode for tests |
| `src/retro/main.cpp` | `opensu-retro`'s command line |
| `src/library/libretro_cores.*` | Finding a core; core downloads from the libretro buildbot under `<data>/opensu/cores/` and their record |
| `src/library/romset_dat.*` | An arcade core's ROM-set listing: a Logiqx datafile or MAME `-listxml` parsed, its line format under the cache |
| `src/library/romset_dats.*` | The arcade cores' listings fetched over HTTPS from the catalog's `"romsets"` and kept under `<cache>/romsets/` for 30 days |
| `src/library/romset_match.*` | An arcade zip's central directory against each listing, the verdicts kept by file stamps, the game's emulator picked by them |
| `assets/emulators.json`, `src/library/emulator_catalog.*` | A built-in emulator is an entry with `"core": "<name>_libretro"` and no launchers or release |
| `src/library/emulators.*` | A built-in emulator is installed when its core is found and `opensu-retro` is beside `opensu` (`config::retroRunnerBeside`); it starts as `opensu-retro --core <core> <rom>`, with `--hidden` from a hidden shell |
| `src/app/emulator_downloads.*`, `emulator_settings.*`, `details_controller.*` | Download, update check, update and removal of a core, on the same paths as an AppImage |
| `src/app/retro_session.*` | The shell's side of the control channel: each launch's control file, the port read from it, the requests, the load failure handed over and the file removed |
| `src/app/quick_menu_controller.*` | Save state, Load state and Reset game while the running game answers on a control channel |
| `src/app/game_end.*`, `src/launch/handoff.*` | How a game ended (`Hooks::ended`, the scope's exit status) and the toast it gets |

`retro` depends on `gamepad`, `fileio`, Lucent, nlohmann/json, SDL3, the Vulkan loader and libpng; `config` is
read by `main.cpp` only. Nothing outside `retro` and the `opensu-retro` executable links it. Only
`presenter` and `vulkan_device` include Vulkan or SDL video; `audio_output` includes SDL audio;
`joypad` uses SDL's scancode values only.

## 4. Contracts

**Command line.** `opensu-retro --core <path.so> [--saves <dir>] [--system <dir>] [--hidden]
[--validate] [--frames N] [--frame-png <file>] <rom>`. `--saves` defaults to
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
into a quit so save RAM is written) or the frame count is reached; 1 for a bad command line or a
window, Vulkan or frame-file failure; 2 when the core or game cannot be loaded (the reason logged);
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
exits. When the core or game cannot be loaded (exit 2) it writes `{"error": "<reason>"}` there
instead and leaves it; the shell reads it once the launch is over and deletes it. Routes:

| Route | Answer |
| --- | --- |
| `GET /status` | `{"core", "rom", "frame", "hasState"}`: the core's name, the ROM path, frames run, whether the slot holds a state |
| `POST /state/save` | 200; 409 with the reason when the core cannot serialize (`retro_serialize_size` 0 or `retro_serialize` false); 500 when the slot cannot be written |
| `POST /state/load` | 200; 404 when no state is saved; 409 when the core refuses it |
| `POST /reset` | 200 after `retro_reset` |

Every POST is queued for the frame loop, which runs it between frames, never beside `retro_run`;
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
runner's reason as an error.

**Input.** The runner reads every evdev pad without grabbing it. While the shell holds the real
pads during a game, the only pad that moves is the shell's virtual Xbox 360 pad, so the Guide
chords stay the shell's and every core sees the same layout. Run alone, it reads the real pads
through the same translator (`gamepad::Pads` with `OwnVirtualPads::Read`, never holding). Pad
positions follow the Xbox layout: A (bottom) is libretro's B, B is A, X (left) is Y, Y is X; L1/R1
are L/R, L2/R2, L3/R3, Select, Start and the d-pad and left stick are themselves; Guide reaches no
core. Keyboard: arrows, Z (B), X (A), A (Y), S (X), Q/W (L/R), Enter (Start), Right Shift (Select);
Escape quits. Port 0 only. Analogue triggers produce no L2/R2 (the translator reports L2/R2 only
for digital triggers), which matters for PCSX ReARMed.

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
update and remove. Built-in entries come after the standalone ones in the catalog, so a system's
automatic emulator stays the standalone one where it is installed.

**Romsets.** An arcade core's catalog entry names its listing: `"romsets": {"files": [...]}`
(HTTPS datafiles merged in order) or `"romsets": {"release": {api, asset, downloads}}` (the latest
release's zip holding one `.xml`, size and SHA-256 checked). `RomsetDats::refresh` fetches a
listing that is missing or older than 30 days, every redirect hop on the file's origin, and keeps
the old one when the fetch fails; a matcher tries again no sooner than an hour later. A zip's set is
its stem, lower case. A set matches a core when every ROM it needs (a `nodump`, optional or
CRC-less ROM is not needed; of BIOS-choice ROMs only the default choice's) is found by CRC-32 and
size in the zip, its `romof` chain's zips or its devices' zips (a device's own devices included)
in the same folder, and each disk is at `<set>/<disk>.chd`. `applyRomsets` keeps the search's
emulator when it matches, else takes the first matching one installed, else names the first
matching one as not installed (the details page offers its download); with every core checked and
none matching the game's `launch` is empty and `unavailable` is "No arcade core matches this
romset", with `romsetNearest` the core that lacks the fewest files and which. A core whose listing
cannot be had is unchecked and changes nothing. `EmulatorChoices::apply` skips a system's pick the
romset does not match; a game's own pick always applies. Verdicts are kept in
`<cache>/romsets/matches.json` with the stamp (size, modification time) of the zip and of every
file the match looked at, absent ones included, and the listings' source CRC-32s; a scan reads
nothing more of a zip whose stamps all hold.

## 5. Phases

1. Software cores: mGBA (GB, GBC, GBA), then Gambatte, snes9x, Mesen, PCSX ReARMed, and for
   Arcade FinalBurn Neo, MAME 2003-Plus, MAME 2010 and MAME, picked per game by romset. Genesis Plus
   GX and PicoDrive wait for Mega Drive, Master System, Game Gear, Sega CD and 32X systems in
   `rom_systems`. Runner, input, audio, save RAM, core downloads through Settings › Emulators and the
   details page.
2. Built: save states and reset from the quick menu over a loopback `lucent::http::Server` in the
   runner; a crash named in the shell's toast, a load failure told by its reason.
3. GPU cores (`RETRO_ENVIRONMENT_SET_HW_RENDER` with `RETRO_HW_CONTEXT_VULKAN`, handed the
   presenter's device): Mupen64Plus-Next, Beetle PSX HW, Flycast.

## 6. Testing

- `tests/retro/` builds three fake cores as shared libraries from one source: one draws a known
  pattern, plays a tone, reads the joypad, writes save RAM and serializes its save RAM and frame
  counter (reset zeroes the count); one crashes in `retro_run`; one cannot serialize.
- Unit tests through the shipping code: pixel conversion, resampler fill control, joypad mapping,
  save RAM round trip and atomic write, environment answers.
- `core_test` loads the fake core through `Core` with the real owners: frames, samples, the
  joypad and save RAM reach them, a second core and a missing library or ROM are `CoreError`s.
- `runner_test` runs `opensu-retro --hidden --validate --frames 30 --frame-png` against the fake
  core (RGB565 bars letterboxed in the PNG, the `.srm` holding the frame count, a second run
  continuing from it), exit 2 for a missing core or ROM, exit 1 for a bad command line, and the
  crashing core killing the runner by SIGSEGV with the last save intact. With `OPENSU_RETRO_CONTROL` set: the control file
  appears with the runner's pid and port, `/status` names the game, loading before a save is 404, a
  save writes the `.state`, a later load leaves the `.srm`'s frame count showing the frames between
  them undone, a reset zeroes it, the stateless core's save is 409 with its reason, an unknown
  route 404, SIGTERM exits 0 and removes the file, and a missing ROM exits 2 leaving the error.
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
  kept verdicts and what invalidates them, the pick and the no-match text.
- A real core and ROM are checked by hand on this machine, never in CI (no ROMs in CI).
