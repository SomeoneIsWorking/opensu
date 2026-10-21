# Shaders, filters, overlays and per-console/core/game settings for opensu-retro

Status: open, design. Nothing built.

The player: "filters and shaders like RetroArch", "overlays", "selectable per console/core/game,
different config layers with override support", and "core per console configuration and per game
override".

## Where the runner stands

- `retro/presenter` is raw Vulkan, not gpu2d or SDL GPU: its own render pass, one pipeline
  (`shaders/frame.{vert,frag}`, glslc at build time), a fixed `VK_FILTER_NEAREST` sampler, the frame
  letterboxed by `letterbox()` to the core's `aspect_ratio`. There is no scaling choice.
- `retro/core_options` holds the core's defaults only; `GET_VARIABLE_UPDATE` never reports a change.
- The runner reads no settings: the shell starts it as `opensu-retro --core <so> <rom>`
  (`library/emulators`), and `config` is read by `main.cpp` only.
- openSU has no touch UI; the runner takes no mouse or touch (`RETRO_DEVICE_POINTER` reads 0).
- `docs/design/libretro.md` lists shaders as a non-goal; that line goes when stage 1 lands.

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

- `Layer { Global, System(key), Core(core name), Game(id) }`. Resolution per key: Game → Core →
  System → Global → built-in default. Each layer keeps only the keys it sets, so "inherit" is
  just a missing key and "Clear" deletes it.
- `resolve(GameKeys{game, system, core})` returns each key's value and the layer it came from. The
  UI shows that layer, and the quick menu saves to a chosen one.
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
- Live changes from the quick menu go over the control channel:
  - `POST /video` re-plans the presenter between frames.
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
  - The folder listing shows no digest for asset zips. That is the same trust level as cores,
    and is to be checked before building.
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
  options (all systems)" for the Core layer. "Everywhere" (Global) is one page at the top.
- **Details page** (`app/details_controller`): "Game settings" with the same rows. Each value
  reads "From Super Nintendo", "From snes9x", "Automatic (romset)" or "Set for this game". The
  row's action menu has "Clear (use <layer below>)".
- **Quick menu** (`app/quick_menu_controller`, only while a game answers on `opensu-retro`'s
  channel): a Display group (Aspect, Integer scale, Sampling, Filter, Shader, its parameters as
  sliders, Bezel) and Core options. Changes apply live and are marked unsaved. Each row shows its
  source layer. "Save to…" offers This game, the system, the core, or Everywhere, and "Clear game
  overrides" removes the Game layer. Closing with unsaved changes keeps them for this session only
  and says so in a toast.
- Shader and bezel pickers browse `<data>/opensu/shaders/slang/` and the overlay folders. The
  built-ins are listed first.

## Build order

1. Split `retro/presenter` (829 lines) at the composition boundary: window and swapchain stay;
   a new `retro/frame_compositor` takes the frame, the plan (rect, passes, bezel) and the target.
   This goes in its own commit.
2. `settings/setting_layers` with `video.aspect`, `video.integer` and `video.sampling` (with
   sharp-bilinear) at Global, System and Game, passed by `--settings`. Details-page and Settings ›
   Emulators rows.
3. Move `emulator_choice` into the layers (System and Game, romset as the game's automatic value)
   and migrate `settings.json`; then add the Core layer and `mapping_scopes` on the shared `Layer`.
4. Built-in filters: scanlines, LCD grid, a CRT ported from crt-lottes.
5. Quick menu live changes and "Save to…" (`POST /video`).
6. Core options per layer (`options.<core>.<key>`), `GET_VARIABLE_UPDATE`, `POST /options`.
7. librashader from a pinned fork, the `shaders_slang.zip` download, preset parameters as sliders.
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
