# Shaders, filters, overlays and per-console/core/game settings for opensu-retro

Status: open. Build order steps 1–5 built; step 7 built except preset parameters; steps 6, 8 and
9 not started.

The player: "filters and shaders like RetroArch", "overlays", "selectable per console/core/game,
different config layers with override support", and "core per console configuration and per game
override".

## Where the runner stands

- `retro/presenter` owns the window, swapchain or hidden offscreen image, submission and read-back;
  `retro/frame_compositor` owns what is drawn: render pass, frame texture, one pipeline, the
  sampler and the placement (`placeFrame`). Both are raw Vulkan, not gpu2d or SDL GPU.
- Aspect (core, 4:3, pixel 1:1, stretch), integer scale (off, on, overscale), sampling (nearest,
  bilinear, sharp-bilinear in `shaders/sharp_bilinear.frag`) and filter (none, scanlines, LCD grid,
  CRT; `retro/frame_filter`) come from `--settings <file>`, which the shell writes per launch.
- `retro/core_options` holds the core's defaults only; `GET_VARIABLE_UPDATE` never reports a change.
- openSU has no touch UI; the runner takes no mouse or touch (`RETRO_DEVICE_POINTER` reads 0).

## What steps 1–3 built

- `settings/setting_layers`: `Layer{Global, System, Emulator, Game}`; the core layer is the
  `Emulator` kind keyed by catalog id (`snes9x-core`). Resolution Game → Emulator → System → Global
  → default; `Resolved::from` names the layer. Stored by `settings/game_settings_store` in
  `game-settings.json` (`global`, `systems`, `emulators`, `games`).
- `settings/video_settings`: the three keys, their choices and labels, `resolveVideo`, the
  per-launch file. `app/retro_launch` resolves them for a game on a built-in core and adds
  `--settings`; `app/retro_session` names and removes the file.
- `library/emulator_choice`: the `emulator` key at the Game and System layers; source Game, Romset
  (the game layer's automatic value), System or Search. `settings/emulator_pick_migration` moves
  `settings.json`'s `emulators` and `systemEmulators` into the store once and drops the keys.
- `input/mapping_scopes` and `settings/mapping_store` use the same `Layer`, keep the whole-map rule
  and gained the System layer.
- UI: Settings › Emulators lists a Global row, one per served console (emulator and video)
  and one per built-in core (video); each opens a page (`app/layer_settings`) where a value steps
  through "not set" too, with a Clear row. The details page (`app/details_controller`) gives a game
  on a built-in core a row per value a layer keeps ("From Super Nintendo", "Set for this game"),
  the emulator's source on its row ("Automatic (romset)", "From <console>", "Set for this game"),
  a "Game settings" button that opens the tabs of step 5 and "Clear game settings" while the game
  keeps any.
- Video settings apply to built-in cores only; standalone emulators keep their own.

## What step 4 built

- `video.filter` (`none`, `scanlines`, `lcd-grid`, `crt`) in `settings/video_settings`, at every
  layer, with its row on the Settings › Emulators pages and its button and source row on the
  details page (both iterate `videoKeys`, so nothing else changed there).
- `retro/frame_filter` picks the fragment shader for the sampling and filter and builds the push
  block; the parameters are `inline constexpr` in `frame_filter.hpp` (`scanlineGrid`, `lcdGrid`,
  `crtLottes`). No user parameters yet.
- Scanlines and LCD grid (`shaders/frame_grid.glsl`, included by `frame.frag` and
  `sharp_bilinear.frag`): a dark band of fixed width in source pixels at the bottom (scanlines:
  0.5 px, shade 0.5) or the right and bottom (LCD: 0.25 px, shade 0.45) of every source pixel. Each
  output pixel is darkened by the exact share of its footprint the bands cover, so the bands stay
  on source pixel edges at any scale, integer or not, and compose with every sampling.
- CRT (`shaders/crt_lottes.frag`): crt-lottes ported from libretro slang-shaders `890a1e95`
  (public domain), its defaults; it samples the source itself, so the sampling setting does not
  change it. Fetches off the frame are black (RetroArch's border sampler) and the shadow mask runs
  on target pixels.
- With the filter at None the frames are pixel-identical to before (Kirby frame 600, nearest,
  bilinear and sharp-bilinear, `magick compare -metric AE` 0).
- Proof: `scratch/video-filters/` (`runner/`, `crops/`, `sheets/`, `grid-check.txt`, `shell/`,
  `launch/`).

## What step 7 built

- librashader comes from `SomeoneIsWorking/librashader` (forked from SnowflakePowered `07ac790c`,
  v0.12.1), pinned in `cmake/Librashader.cmake`. The fork's commits:
  - `73bf79af`: each pass's scissor is clipped to non-negative offsets, so an overscaled viewport
    that overhangs the output draws instead of failing validation.
  - `41211fab`: a pass's render passes, pipeline layout, descriptor pool and set layout, and each
    LUT's image and view, are destroyed with the chain (upstream leaks them; validation reports
    them at `vkDestroyDevice`).
- The build runs `cargo build --locked --profile optimized --features runtime-vulkan` for
  `librashader-capi` (Rust 1.88 or newer; refused with the install command otherwise, or
  `-DOPENSU_BUILD_LIBRASHADER=OFF`), stages `<libdir>/opensu/librashader.so` in the build tree and
  installs it there. The header comes from the same revision, checked by SHA-256.
- `retro/librashader_library` opens that file by its path (`Config::librashader`, beside the
  runner), checks ABI and API, and turns librashader errors into `ShaderError`. Without the file
  the runner refuses: "shader presets need librashader, which is not installed (<path>)"; the
  shell shows that as the launch's error toast. It never draws without the preset.
- `retro/shader_chain` builds the preset on the runner's device (render passes, one frame in
  flight) and records it from the frame texture into the placed rectangle of the target;
  `retro/frame_compositor` uses it in place of its own pipeline when `video.shaderPreset` is set.
  The frame texture carries `TRANSFER_SRC` because librashader copies it into `OriginalHistory`,
  and the first frame clears the history images.
- `video.shader` at every layer holds a path in the pack (`crt/crt-royale.slangp`); `""` is an
  explicit None, so a game can turn off a console's preset. The shell resolves it against
  `<data>/opensu/shaders/slang/` and writes the absolute path into the launch file.
- How it combines with the built-ins: a preset replaces Sampling and Filter (the rows say "not
  used with a shader preset" and the details buttons read "Not used"). Aspect and Integer scale
  still place the preset's output rectangle.
- The pack (`library/shader_pack`): Settings › Emulators has a "Shader presets" row (Download,
  progress, Installed, Up to date, Update to <date>) and a two-press Remove row; the download shows
  on the Downloads page as a "Shaders" row with bytes. Check for updates reads the pack too.
  - The buildbot publishes no checksum for `shaders_slang.zip`: `.md5`, `.sha256`, `.sha1` and
    `.index-extended` are 404 and the listing gives only date and size. What is checked: HTTPS from
    the buildbot origin only, the body length against the response's `Content-Length`, and every
    member's CRC-32 as it is unpacked. Members that would land outside the pack are refused.
  - The version is the response's `Last-Modified`; the record (`shaders/slang.json`) keeps that,
    the size and the URL. The buildbot's CDN edges serve different builds at once (54283163 and
    54283881 bytes, 10:01 and 14:05 on 2026-10-21), so Check for updates can offer an update a
    download then does not get; the row then still says "Update to".
  - Unpacked into `slang.part`, then swapped for `slang/`, so a failed download keeps the old pack.
- The picker (`app/shader_preset_picker`) walks the pack's folder tree (5210 files, 2659 presets;
  `bezel/` alone has 1494, so a flat folder list does not work): "Not set" and "None" at the top,
  then each level's folders with their preset counts and its presets. From a layer page it
  replaces the page; from the game settings tabs it replaces the tab's rows and B goes back to
  them.
- Not built: preset parameters as sliders (`libra_preset_get_param`/`set_param`, `GET /status`). The AppImage and CI containers gained Rust
  (`packaging/appimage`: rustup-init 1.28.2 by SHA-256, toolchain 1.90.0; `packaging/ci`: Fedora's
  `cargo`) but neither image has been rebuilt with it.
- Proof (`scratch/shader-presets/`, hidden, scratch HOME and XDG dirs, validation on, 0 errors in
  every run): `shell/03`–`08` and `22`–`26` (row before, during, after, update, remove, Downloads page
  during and after),
  `shell/09`–`15` (Super Nintendo layer → crt-royale, nested folders), `shell/16`–`20` (Chrono
  Trigger overridden to sharp-bilinear-simple on its details page, Super Metroid inheriting),
  `launch/*.settings.json` (the files the shell wrote), `runner/*.png` and `runner/*.log`
  (Super Metroid crt-royale with and without overscale, Chrono Trigger sharp-bilinear-simple, Kirby
  GBA lcd-grid-v2, crt-guest-advanced, sharp-bilinear-simple and gba-lcd-grid-v2 with its border
  and history), `runner/missing-library.log` and `shell/21-*` (the refusal and its toast).
- The task named `interpolation/sharp-bilinear-simple`; in the pack it is
  `pixel-art-scaling/sharp-bilinear-simple.slangp`.

## What step 5 built

The player turned down "Save to…": "Save to... doesn't sit with me because per setting override,
maybe we can add tabs, global / system / game and if not in global, they can have an inherit option
or an override checkbox". So there is no unsaved state and no save step.

- One page, two doors: the quick menu's "Game settings" row (while a game runs on `opensu-retro`)
  and the details page's "Game settings" button open `app/game_settings_page` on the Game tab. It
  replaces the details page's per-button "not set" cycling.
- Tabs Global, the game's console ("Super Nintendo") and Game, each with Aspect, Integer scale,
  Sampling, Filter and Shader preset. L1/R1 switch tabs, A ticks Override or opens the value, Left
  and Right move between the checkbox and the value, B backs out (picker, then list, then the page,
  back to the quick menu or the details page). A tap is focus plus A, a tap on a tab shows it.
- Global rows are plain values. System and Game rows carry an Override checkbox: unticked, the row
  shows the value it inherits, dimmed, and where it comes from ("From Global", "From Snes9x
  (built-in)", "Default"); ticking copies that value into the layer and opens it for editing;
  unticking deletes the key. A row a higher layer overrides says so ("Overridden by Snes9x
  (built-in)"); with a preset, Sampling and Filter say "Not used with a shader preset".
- The core layer is not a tab. It sits between System and Game, stays editable in Settings ›
  Emulators, and shows here only as a source. The System tab inherits from Global and the default
  only (`SettingLayers::inherited`, `SettingLayers::over`).
- Every change is written to `game-settings.json` at once; while that game runs the shell resolves
  its layers again (`app/retro_launch: retroVideoOf`) and sends them as `POST /video` through
  `app/retro_session: setVideo`. The runner (`retro/runner_app`) applies them between frames:
  `FrameCompositor::change` builds a new preset chain first, so a bad preset answers 409 and keeps
  what is drawn, then rebuilds the pipeline. The body is `settings::videoJson`, the launch file's
  format.
- `GET /frame.png` on the runner's channel (hidden runs only, 409 otherwise) returns the last frame;
  the proof reads runner frames there.
- Proof (`scratch/settings-tabs/`, shell `--hidden`, scratch HOME and XDG dirs, Chrono Trigger
  and Super Metroid from `/mnt/Boy/ROM/SNES`; the shell's frame shows the panel over grey, since a
  hidden shell does not show the runner's window):
  - `proof/02`–`04`: the details page's tabs; `proof/11`–`14`: the quick menu and its tabs over
    the running game.
  - `proof/20`–`22`: System Filter ticked (store gains `systems.snes.video.filter: none`), set to
    Scanlines; `22-runner-system-scanlines.png` shows it live.
  - `proof/23`–`26`: Game tab shows "Scanlines · From Super Nintendo", ticked, set to CRT;
    `26-runner-game-crt.png` shows the CRT mask live.
  - `proof/27`: unticked, the store drops the game's key and `27-runner-game-unchecked.png` is
    back to scanlines; the shell log's `video now` lines show each change.
  - `proof/31`–`33`: Super Metroid launched afterwards draws scanlines and its Game tab reads
    "Scanlines · From Super Nintendo".
  - `runner/smoke-*`: `POST /video` none → crt → crt-royale → none on a hidden runner with
    validation on (0 errors), a bad value (400) and a missing preset (409, settings kept).
  - Each `proof/*.json` is the store after that step.
- Not proven: touch on the tabs (the control channel has no pointer input), the quick menu over a
  game nested in Gamescope.

## Layered settings: one owner

### What exists

| Owner | Layers | Semantics |
| --- | --- | --- |
| `input/mapping_scopes` | game → emulator (catalog id) → global | the whole map replaces the one below it; `source()` names the layer that applies, `clear()` drops a layer |
| `library/emulator_choice` | game → system, with a romset rule | a game's pick wins; a system's pick gives way to the core the arcade romset matches (`RomsetFit::Mismatch`/`Pending`); stored in `settings.json` (`emulators`, system emulators) |
| `retro/core_options` | none | defaults only, nothing stored |

None of them merges per setting, and none has all four layers. Neither is a clean base on its own,
so the design adds one owner and moves `emulator_choice` into it. A third layered mechanism next
to these would break the one-owner rule.

### The owner: `settings/setting_layers` (new), store `<config>/opensu/game-settings.json`

- `Layer { Global, System(key), Emulator(catalog id), Game(id) }` (the core layer is `Emulator`).
  Resolution per key: Game → Emulator → System → Global → built-in default. Each layer keeps only the keys it sets, so "inherit" is
  just a missing key and "Clear" deletes it.
- `resolve(LayerKeys{game, system, emulator}, key, default)` returns each key's value and the layer it came from. The
  UI shows that layer; the game settings tabs write each change to the tab's layer.
- Keys:
  - `emulator`: System and Game layers only, since the core is what picks the Core layer.
    Resolution: game's manual pick → the game's automatic value (the core its romset matches,
    from `library/romset_match`) → system pick → what the search found. This is how
    `EmulatorChoices::apply` already behaves, now stated as layers. The UI shows the romset's
    choice as "Automatic (romset: FinalBurn Neo)" at the game layer, and a manual pick overrides it.
  - `video.aspect`, `video.integer`, `video.sampling`, `video.filter`, `video.shader` (preset
    path), `video.shaderParams.<name>`, `video.overlay` (cfg path), `video.overlayOpacity`.
  - `options.<core>.<key>`: core options, always keyed by core at every layer, so a system's
    snes9x values never reach a game moved to bsnes. The Core layer is their natural home. System
    and Game layers may set them too.
- The core is resolved first because `emulator` decides which Core layer and which `options.<core>`
  apply.
- `input/mapping_scopes` keeps its whole-map contract (one visible map) and switches to the same
  `Layer` type, gaining a System layer. The two stores stay separate because a mapping is one value.
- Migration: on first load, `settings.json`'s per-game and per-system emulator picks move into the
  new store and their keys are dropped from `settings.json`. There is no dual read afterwards.
- Order note: the player asked for console → core. A multi-system core (mGBA: GB, GBC, GBA) has a
  Core layer that sits over every one of its systems, so video belongs at the System layer and
  options at the Core layer. Both work anywhere.

### RetroArch, for comparison

- Config overrides go game → content directory → core → `retroarch.cfg`, under
  `config/<core>/<core>.cfg`, `<content dir>.cfg` and `<game>.cfg`. Only changed values are saved.
  Remaps (`.rmp`) follow the same order.
- Shader presets are saved per core, content directory or game as `.slangp`.
- Core options live in `retroarch-core-options.cfg`, with game (and content-directory) `.opt` files
  over it. [docs.libretro.com/guides/overrides]
- Where we differ:
  - We have a System layer, because openSU's library is by system and RetroArch is by core.
  - We have no content-directory layer.
  - Every layer of every key lives in one store with the source shown, instead of files spread
    over folders.
  - Core options are keyed by core at every layer.

### Shell to runner

- At launch the shell resolves the layers (beside `MappingEditor::forGame`) and writes the
  resolved values to a per-launch file next to the control file. It passes that file as
  `opensu-retro --settings <file>`, which `main.cpp` reads into typed options.
- Live changes from the game settings tabs go over the control channel:
  - `POST /video` (built) takes the resolved video settings and rebuilds the compositor between
    frames.
  - `POST /options` sets the values in `CoreOptions`, and `GET_VARIABLE_UPDATE` then returns true
    once.
  - `GET /status` gains the preset's parameters (name, label, min, max, step, value).
- The runner never reads `game-settings.json`.

## Scaling and filters

- RetroArch's video filters are CPU `.filt` plugins in `gfx/video_filters`: Scale2x, 2xSaI,
  SuperEagle, LQ2x, EPX, Blargg NTSC variants, Dot_Matrix/Gameboy grids, Upscale_*, crop_borders,
  dedither, pixel_art_aa. They are GPL, built against `softfilter.h`, and run on the CPU before
  upload, software cores only. They are not worth offering: every one of these looks exists as a
  slang shader on the GPU, and the CPU cost lands where Deck-class hardware has the least to spare.
- Worth offering, all in the presenter:
  - Aspect: core, 4:3, pixel 1:1, stretch.
  - Integer scale: off, on, on with overscale.
  - Sampling: nearest, bilinear, sharp-bilinear (integer prescale then bilinear).
  - Built-in single-pass filters: scanlines, LCD grid (handhelds), one CRT.
  - These are compiled by glslc at build time like `frame.frag`.
- Licences of the built-in filters: openSU is MIT. GPL shaders (crt-geom, crt-royale and
  crt-easymode are GPL-2+ or GPL) are not copied into the tree. crt-lottes says "PUBLIC DOMAIN"
  and can be ported. Scanlines and the LCD grid are small enough to write ourselves.

## Shaders: slang presets

- Format: a `.slangp` file has these parts.
  - `shaders = N` and `shaderN`, with per-pass keys `filter_linearN`, `wrap_modeN`,
    `scale_typeN`, `scale_xN`/`scale_yN`, `mipmap_inputN`, `float_framebufferN` and
    `srgb_framebufferN`, plus `aliasN`/`#pragma name` and `frame_count_modN`.
  - LUTs as `textures = "a;b"` with `NAME_linear`, `NAME_wrap_mode` and `NAME_mipmap`.
  - Parameter overrides as `NAME = value`, and `#reference` chains.
- A `.slang` file is Vulkan GLSL with `#pragma stage`, `#pragma parameter ID "desc" init min max
  [step]`, `#pragma format` and `#include`. Its semantics:
  - textures `Original`, `Source`, `OriginalHistoryN`, `PassOutputN`, `PassFeedbackN`, `UserN`;
  - uniforms `MVP`, `OutputSize`, `FinalViewportSize`, `FrameCount`, `FrameDirection`,
    `Rotation`, `OriginalAspect`, `OriginalFPS`, `FrameTimeDelta`.
  - One UBO and one push-constant block, 128 bytes guaranteed.
  - Compiled by glslang to SPIR-V and reflected with SPIRV-Cross.
  - [docs.libretro.com/development/shader/slang-shaders]
- (a) librashader (github.com/SnowflakePowered/librashader).
  - Licence: runtime MPL-2.0 (or GPL-3 at the distributor's choice), C headers MIT.
  - Linking: loaded at run time through the single-header `librashader_ld.h`, or linked
    dynamically. Static linking is not supported. Building it needs Rust ≥ 1.88.
  - The Vulkan runtime takes raw handles:
    - `libra_device_vk_t {VkPhysicalDevice, VkInstance, VkDevice, VkQueue, PFN_vkGetInstanceProcAddr}`
      for `libra_vk_filter_chain_create`;
    - `libra_vk_filter_chain_frame(chain, VkCommandBuffer, frame_count, libra_image_vk_t in, libra_image_vk_t out, viewport, mvp, options)`.
    - The input must be `SHADER_READ_ONLY_OPTIMAL`, and the output is left in
      `COLOR_ATTACHMENT_OPTIMAL`.
    - `VK_KHR_dynamic_rendering` is optional and must be enabled at device creation.
    - `libra_preset_create_with_options` and `libra_preset_set_param` / `get_param` handle
      presets and parameters.
  - The runner already has every one of those handles (`retro/vulkan_device`; the presenter's
    `commands_`, `texture_` and swapchain or offscreen image). No interop is needed.
  - On gpu2d it would not work: SDL 3.4.18 (installed) gives `SDL_GetGPUDeviceProperties` with
    name and driver strings only, and no `VkDevice`, `VkImage` or `VkCommandBuffer`. Using it there
    would need SDL to add native-handle getters, or gpu2d to own its Vulkan device and hand it to
    SDL. That is moot for the runner.
  - Cost: a pinned fork built with cargo in our provisioning, one more `.so` shipped, and history,
    feedback, LUTs and parameters done by a maintained implementation.
- (b) Our own runtime. It needs:
  - glslang at run time, since presets are source;
  - SPIRV-Cross or SPIRV-Reflect;
  - the preprocessor (`#include`, pragmas, `#reference`) and the preset parser;
  - scale types, aliases, the history ring, feedback buffers, LUTs (libpng is already linked),
    mipmaps, float and sRGB targets, and parameter blocks.

  Several thousand lines, which is what librashader already is. Not recommended.
- (c) Curated built-ins only. Small, and they cover what most players pick. They are the
  "Scaling and filters" list above, and they do not open the libretro packs.
- Recommendation: (c) first, then (a) for the packs. Not (b).
- Packs:
  - libretro's buildbot ships `https://buildbot.libretro.com/assets/frontend/shaders_slang.zip`
    (54 MB on 2026-10-21; also `overlays.zip`, 27 MB).
  - It would be downloaded on request through the core download path (HTTPS, same-origin
    redirects, a Downloads page row) into `<data>/opensu/shaders/slang/`, with a record for
    update and removal.
  - The buildbot publishes no digest for it (see "What step 7 built" for what is checked).
  - `libretro/slang-shaders` has no repository licence (GitHub reports none). Each shader carries
    its own: crt-lottes public domain, crt-geom GPL-2+, crt-royale GPL-2+, crt-easymode GPL, many
    with no header at all. The player downloads them at run time like cores, and the tree never
    holds them.

## Overlays and bezels

- Format (`.cfg`, docs.libretro.com/development/retroarch/input/overlay):
  - `overlays = N`, `overlayN_overlay` (image), `overlayN_rect`, `overlayN_full_screen`
    (screen instead of viewport), `overlayN_normalized`;
  - `overlayN_descs = K`, `overlayN_descK = "button,x,y,radial|rect,range_x,range_y"`;
    `left|up` presses several buttons; `analog_left`/`analog_right` radial with `saturate_pct`;
    `dpad_area`/`abxy_area`; per-descriptor images, `movable`, `alpha_mod`, `range_mod`,
    `reach_*` and `exclusive`;
  - `overlay_next` / `next_target` to switch between overlays.
- Bezels are `descs = 0` overlays.
  - common-overlays' borders give the game rect explicitly: `borders/gb.cfg` has
    `overlay0_full_screen = true` and `overlay0_viewport = "0.19,0.0,0.636,1.0"`.
  - The Bezel Project's system cfg gives only `overlay0_overlay = <png>`, `full_screen = true`
    and `descs = 0`. Its per-game cfgs are `input_overlay = "/opt/retropie/..."`. It relies on the
    aspect and viewport its installer writes into `retroarch.cfg`.
- Licences:
  - `libretro/common-overlays` (the buildbot's `overlays.zip`) is CC-BY-4.0, so it can be
    downloaded and credited in the UI.
  - `libretro/overlay-borders` is MIT.
  - The Bezel Project repos (`thebezelproject/bezelproject-<System>`, SNES alone 529 MB) have no
    licence and carry box-art and trademark-derived art. Offer "Use a folder" import only, never a
    download or mirror. Their absolute RetroPie paths are rewritten to the folder.
- Layout in the presenter:
  - The bezel image is fitted to the window with its aspect kept.
  - The game rect is the cfg's `overlayN_viewport` mapped into that box. Without one, it is the
    bezel's transparent hole (the largest transparent rectangle, found once at load), else a
    centred box at the core's aspect.
  - Integer scale and aspect apply inside that rect, and the shader chain's viewport is that rect.
  - The bezel is drawn over the game with `overlayOpacity`. A load pass keeps the chain's output.
- Touch overlays: descriptors map to `RETRO_DEVICE_JOYPAD` and analog as one more source in
  `retro/joypad` (beside the control pad), from SDL finger events in the presenter. There is no
  openSU touch UI to reuse or conflict with. They come last: the hardware has pads, and touch over
  the runner inside Gamescope has not been tried.

## UI

All of these use the existing `settings_page` row types (choice, toggle, slider, action, info).

- **Settings › Emulators** (`app/emulator_settings`): the emulator-per-system rows become a page
  per system with Emulator, Aspect, Integer scale, Sampling, Filter, Shader preset, Bezel, Bezel
  opacity, and Core options (a sub-page from the system's core definitions, which the shell learns
  from the runner's `/status`, or from the last run kept per core). A core's page gets "Core
  options (all systems)" for the Core layer. Global is one page at the top.
- **Details page** (`app/details_controller`): "Game settings" with the same rows. Each value
  reads "From Super Nintendo", "From snes9x", "Automatic (romset)" or "Set for this game". The
  row's action menu has "Clear (use <layer below>)".
- **Quick menu** (`app/quick_menu_controller`, only while a game answers on `opensu-retro`'s
  channel): a "Game settings" row opens the tabs of step 5; changes apply live and are saved to
  the tab's layer at once. Still to come there: preset parameters as sliders, Bezel, Core options.
- The shader picker browses `<data>/opensu/shaders/slang/` by folder; the built-in filters keep
  their own Filter row. The bezel picker will browse the overlay folders.

## Build order

1. Done: split `retro/presenter` at the composition boundary into the presenter (window,
   swapchain) and `retro/frame_compositor`. Runner frames byte-identical before and after.
2. Done: `settings/setting_layers` with `video.aspect`, `video.integer` and `video.sampling` (with
   sharp-bilinear), passed by `--settings`. Details-page and Settings › Emulators rows.
3. Done: `emulator_choice` in the layers, `settings.json` migrated, the core layer, and
   `mapping_scopes` on the shared `Layer`.
4. Done: built-in filters: scanlines, LCD grid, a CRT ported from crt-lottes.
5. Done: the Global / System / Game settings tabs with Override checkboxes, in the quick menu and
   on the details page, applied live through `POST /video`.
6. Core options per layer (`options.<core>.<key>`), `GET_VARIABLE_UPDATE`, `POST /options`.
7. Done except parameters: librashader from a pinned fork, the `shaders_slang.zip` download and
   the preset picker. Preset parameters as sliders not built.
8. Bezels: the common-overlays download, folder import for Bezel Project packs, the game rect from
   the viewport or the hole.
9. Touch overlays.

## Sources

- docs.libretro.com/development/shader/slang-shaders/
- docs.libretro.com/guides/overrides/
- docs.libretro.com/development/retroarch/input/overlay/
- github.com/SnowflakePowered/librashader (README; `include/librashader.h`)
- buildbot.libretro.com/assets/frontend/ (listing, 2026-10-21)
- `gh api` reads of libretro/slang-shaders (licence, shader headers), libretro/common-overlays
  (`COPYING`, `borders/gb.cfg`), libretro/overlay-borders, thebezelproject/bezelproject-SNES,
  libretro/RetroArch `gfx/video_filters`
- `/usr/include/SDL3/SDL_gpu.h` (SDL 3.4.18)
