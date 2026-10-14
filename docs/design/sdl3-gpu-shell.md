# The shell on SDL3 GPU

Status: steps 1 to 5 are done. The shell draws with gpu2d (`gpu2d::render`, `gpu2d::image`, its own
repository, consumed like Lucent) and takes its window, keys, mouse and sound from SDL3; raylib is
gone from the shell and the build. Open: the player's check of the overlay over a game in nested
Gamescope (§5.4).

The window's alpha: SDL 3.4.18's `SDL_ClaimWindowForGPUDevice` refuses any window created with
`SDL_WINDOW_TRANSPARENT` ("The GPU API doesn't support transparent windows", `SDL_gpu.c`), and
without that flag SDL's X11 backend gives a non-GL window the default 24-bit visual. gpu2d's
`GpuWindow` therefore picks a 32-bit TrueColor visual itself and hands it to SDL through
`SDL_HINT_VIDEO_X11_WINDOW_VISUALID`; on such a visual Mesa's X11 swapchain offers only INHERIT and
PRE_MULTIPLIED composite alpha, and SDL takes INHERIT, so the window's premultiplied alpha is what
the compositor blends. gpu2d's `tests/canvas_test` checks the depth; the overlay over a game is
still to be seen by the player through this path (§5.4).

## 1. Goal

The shell draws with SDL3 GPU (Vulkan on Linux) instead of raylib's OpenGL, and takes its window,
keyboard, mouse and sound from SDL3. What the player sees does not change: every screen renders
the same as before within a small per-pixel tolerance.

Non-goals: new visuals, a Windows build (SDL3 GPU keeps that possible), sharing a renderer with
`opensu-retro`, which keeps raw Vulkan so phase 3 cores can draw on its device.

## 2. What raylib did for the shell

| Area | raylib use | Owners |
| --- | --- | --- |
| Window | `InitWindow`, hidden and transparent flags, resize, monitor size and refresh, the X11 window id for the Gamescope overlay | `app/shell_app`, `app/game_screen`, `session/monitor` |
| Drawing | rectangles, rounded rectangles and outlines, circles, rings, lines, triangles, textured quads, `rlgl` transforms and immediate quads, scissor, blend modes | the `ui/*_painter`, `ui/round_shape`, `ui/glass`, `ui/clip_stack`, `ui/vector_icon` |
| Offscreen | render textures for the dock blur and `--render`/`/frame.png`, read back to PNG | `ui/backdrop_blur`, `app/frame_png` |
| Shaders | the blur's two GLSL 330 passes and the capsule mask | `ui/backdrop_blur` |
| Text | `LoadFontEx` atlas of Cal Sans, `DrawTextCodepoint`, `MeasureTextCodepoints` | `ui/typeface` |
| Images | `LoadImage`, `ImageResize`, `ImageFormat`, textures from images (WebP through libwebp already) | `ui/image_decoder`, `ui/glyph_textures`, `artwork/starter_pack` |
| Input | keys pressed, released and repeated, typed text, mouse position, buttons, wheel | `input/keyboard_bindings`, `input/key_names`, `app/pointer_router` |
| Sound | the audio device, WAV and OGG UI sounds | `audio/sound_player` |

The layout model (`ui/home_layout`, `rail_layout`, `dock_metrics`, ...) used its own `Rect` and
did not change.

## 3. Shape after the port

The 2D layer is gpu2d (`$GPU2D_ROOT`, pinned in `tools/check/pins.py`), title-neutral: nothing in it
knows openSU, the fonts and the face options come from the caller. Its paths below are in that
repository.

| Owner | Owns |
| --- | --- |
| gpu2d `gpu_window` | The SDL window and `SDL_GPUDevice` (Vulkan driver), claimed together; hidden, an ARGB visual for the alpha, resize, present mode (vsync, or immediate when hidden); the X11 window number for the overlay |
| gpu2d `canvas` | The 2D drawing API the painters call: filled and outlined rounded rectangles, circles, rings and arcs, lines, triangles, vertex-coloured meshes (`round_shape`'s gradients), textured quads with tint, a transform stack, a scissor stack, blend modes, render targets, the backdrop blur. Recorded per frame, drawn in as few passes as possible |
| gpu2d `mesh_pipeline`, `blur_pipeline`, `pipeline` | Textured vertex-coloured triangles; the dock blur's two passes (`ui/backdrop_blur`'s GLSL 330 ported); pipelines per target format and blend |
| gpu2d `shape_pipeline` | One SDF pipeline for every shape; per-shape data in a storage buffer (uniform pushes over 4 KB read 0 in SDL3 GPU) |
| gpu2d `texture` | GPU textures from RGBA pixels, uploads through transfer buffers, sampler choice |
| gpu2d `render_target` | Offscreen colour targets and their read-back to RGBA |
| gpu2d `text` | A face file through SDL3_ttf (FreeType, HarfBuzz) and its GPU text engine: glyph atlas, measure, draw. The shell passes Cal Sans unhinted (`FaceOptions`) and draws a glyph at a time where `ui/text_grid` puts it, on the pixel grid stb_truetype's atlas used |
| gpu2d `src/shaders/`, `cmake/Gpu2dShaders.cmake` | GLSL compiled to SPIR-V by `glslc` at build time, embedded as headers; `gpu2d_compile_shaders` also builds `opensu-retro`'s |
| `gpu2d::image` | Decoding PNG, JPEG and WebP with SDL3_image, cubic resizing (Mitchell down, Catmull-Rom up, as stb_image_resize2 defaults), and the one PNG encoder (`opensu-retro` and the starter pack use it already), encoding to memory so `/frame.png` stops writing a temporary file under `/tmp`; no GPU |
| `src/audio/sdl_sound_player.*`, `sound_clip.*` | UI sounds on SDL audio streams, one per effect; WAV through `SDL_LoadWAV`, OGG through libvorbisfile, decoded without a device |
| `src/input/keyboard_bindings.*`, `app/pointer_router.*` | SDL keyboard, text and mouse events into the existing raylib-free key names and pointer actions |

`ui/` painters keep their structure and switch their calls to `gpu2d::Canvas`. In openSU nothing
outside `src/audio`, `src/retro` and the window owner includes SDL; the rest reaches it only
through gpu2d.

## 4. Contracts kept

- `--hidden`: an unmapped window on the player's session, no vsync, no sound device, errors logged.
- `--render FILE` and `GET /frame.png`: one frame drawn offscreen and read back, same size.
- Inside Gamescope the window is the overlay: SDL is told to use X11 there (the overlay is an X11
  property on the window) and the window is created with alpha so the game shows through.
- UI scale, the dock blur radius, fonts and colours are unchanged.

## 5. Proving the look

1. Before any change, the current build renders a fixed set of screens to PNG
   (`tools.screenshots --parity DIR`: the README scenes plus a console shelf, Settings pages, the
   options panel, search, the dock glass over artwork, the volume OSD, the session dialog and the
   Downloads page, in a run of its own with a stand-in `legendary`), driven
   through `--hidden` and the control channel with the demo library; each frame is taken once
   `/state` reports `artworkPending` 0, so covers still decoding never read as a difference. These are the references, kept
   under `scratch/`, never committed.
2. After the port the same tool renders the same screens and compares them: mean and maximum
   per-channel difference per screen, and a difference image. Antialiased edges and text may move by
   a pixel; anything above the threshold is a defect to fix, not to accept.
3. Unit tests for the canvas render primitives offscreen and check pixels (a rounded rectangle's
   inside, edge and outside; a ring's gap; scissor; blend).
4. The overlay over a game in nested Gamescope is checked on the player's desktop by the player;
   agents cannot open a visible Gamescope.

## 6. Order

1. Spike: an SDL3 GPU window, transparent, as the overlay inside nested Gamescope, with a rounded
   rectangle and text. This is the one risk the rest depends on.
2. Capture the reference screens (§5.1).
3. Build gpu2d and the SDL sound player with their tests. Done: gpu2d's `canvas_test` (primitives,
   scissor, blend, transform, texture tint, blur, read-back), `text_test` and `image_test`, and
   openSU's `tests/audio/sdl_sound_player_test`, on RADV and lavapipe. `opensu-retro` and the
   starter pack already use `gpu2d::image`.
4. Move every painter, the window, input and sound to them, delete raylib from the shell and the
   build, in one change; compare against the references. Sound: `app/shell_app` holds an
   `audio::SdlSoundPlayer` (same calls: `open`, `load`, `setMuted`, `play`) instead of
   `audio::SoundPlayer`, `opensu_audio_sdl` is folded into `opensu_audio`, and the raylib
   `sound_player.*` and its test are deleted. Known deltas for the comparison: the canvas
   antialiases shape edges where raylib drew them aliased; a render target drawn back is treated as
   premultiplied (raylib multiplied its colour by alpha a second time); text is unhinted and
   unkerned as stb_truetype drew it (`gpu2d::FaceOptions{.hinted = false, .kerned = false}`), but
   rasterised by FreeType. The shell logs what gpu2d reports (`GpuWindow::deviceName`, `driver`,
   `translucent`; `Text::draw` false; decode and encode error strings), since gpu2d never logs.
   Done. Two owners keep raylib's look rather than SDL's defaults: `ui/text_grid` places each glyph
   on stb_truetype's whole-pixel advances and baseline (SDL3_ttf's exact advances made labels
   about half a pixel a glyph wider), and `gpu2d::image` resizes covers with stb_image_resize2's
   cubic filters (SDL's bilinear changed the point-sampled covers). An unmapped window gets no
   swapchain image, so the loop streams artwork itself rather than from the draw. The raylib
   `Typeface` kept 16 sizes and drew any further size from the first one's atlas; the reference
   frames show this at the Settings values and the breadcrumb, which the port draws at their own
   size, so those scenes are compared against a reference with that cache widened.
5. Update `docs/codemap.md` and the owners' docs, and CI and AppImage dependencies (SDL3_ttf,
   SDL3_image; raylib and the raylib fork pin go). Done.
