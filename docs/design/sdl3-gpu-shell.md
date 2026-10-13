# The shell on SDL3 GPU

Status: in progress. The overlay spike (§6.1) passed: an SDL3 GPU window with
`SDL_WINDOW_TRANSPARENT` on X11 gets a 32-bit visual and INHERIT composite alpha, and nested
Gamescope showed the game through it.

## 1. Goal

The shell draws with SDL3 GPU (Vulkan on Linux) instead of raylib's OpenGL, and takes its window,
keyboard, mouse and sound from SDL3. What the player sees does not change: every screen renders
the same as before within a small per-pixel tolerance.

Non-goals: new visuals, a Windows build (SDL3 GPU keeps that possible), sharing a renderer with
`opensu-retro`, which keeps raw Vulkan so phase 3 cores can draw on its device.

## 2. What raylib does for the shell today

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

The layout model (`ui/home_layout`, `rail_layout`, `dock_metrics`, ...) already uses its own `Rect`
and no raylib; it does not change.

## 3. Shape after the port

| Owner | Owns |
| --- | --- |
| `src/render/gpu_window.*` | The SDL window and `SDL_GPUDevice` (Vulkan driver), claimed together; hidden, transparent, resize, present mode (vsync, or immediate when hidden); the X11 window number for the overlay |
| `src/render/canvas.*` | The 2D drawing API the painters call: filled and outlined rounded rectangles, circles, rings and arcs, lines, triangles, textured quads with tint, a transform stack, a scissor stack, blend modes, render targets. Recorded per frame, drawn in as few passes as possible |
| `src/render/shape_pipeline.*` | One SDF pipeline for every shape; per-shape data in a storage buffer (uniform pushes over 4 KB read 0 in SDL3 GPU) |
| `src/render/texture.*` | GPU textures from RGBA pixels, uploads through transfer buffers, sampler choice |
| `src/render/render_target.*` | Offscreen colour targets and their read-back to RGBA |
| `src/render/text.*` | Cal Sans through SDL3_ttf (FreeType, HarfBuzz) and its GPU text engine: glyph atlas, measure, draw |
| `src/render/shaders/` | GLSL compiled to SPIR-V by `glslc` at build time, embedded as headers |
| `src/image/` | Decoding PNG, JPEG and WebP with SDL3_image, resizing, and the PNG encoder now in `retro/png_encoder` (moved, one owner for both executables), encoding to memory so `/frame.png` stops writing a temporary file under `/tmp` |
| `src/audio/sound_player.*` | UI sounds on an SDL audio stream; WAV through `SDL_LoadWAV`, OGG through libvorbisfile |
| `src/input/keyboard_bindings.*`, `app/pointer_router.*` | SDL keyboard, text and mouse events into the existing raylib-free key names and pointer actions |

`ui/` painters keep their structure and switch their calls to `render::Canvas`. Nothing outside
`src/render`, `src/image`, `src/audio` and the window owner includes SDL.

## 4. Contracts kept

- `--hidden`: an unmapped window on the player's session, no vsync, no sound device, errors logged.
- `--render FILE` and `GET /frame.png`: one frame drawn offscreen and read back, same size.
- Inside Gamescope the window is the overlay: SDL is told to use X11 there (the overlay is an X11
  property on the window) and the window is created with alpha so the game shows through.
- UI scale, the dock blur radius, fonts and colours are unchanged.

## 5. Proving the look

1. Before any change, the current build renders a fixed set of screens to PNG
   (`tools.screenshots --parity DIR`: the README scenes plus a console shelf, Settings pages, the
   options panel, search, the dock glass over artwork, the volume OSD and the session dialog), driven
   through `--hidden` and the control channel with the demo library. These are the references, kept
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
3. Build `render/`, `image/` and the SDL sound player with their tests.
4. Move every painter, the window, input and sound to them, delete raylib from the shell and the
   build, in one change; compare against the references.
5. Update `docs/codemap.md` and the owners' docs, and CI and AppImage dependencies (SDL3_ttf,
   SDL3_image; raylib and the raylib fork pin go).
