# Visual presets

Requested 2026-11-07: "visual presets like CRT that applies a CRT shader and a CRT TV overlay" and
"Auto Console preset that automatically applies a preset related to the console like Auto Console
applies NES overlay to NES games". Then: "you don't have to bundle in the assets, they can be
downloaded on demand, also would need portrait specific preset layouts".

## What it does

- A visual preset is one choice that sets several video settings together: a shader preset, an
  overlay image around the picture, and the aspect and scaling they need. CRT is the first: a CRT
  shader from the pack and a CRT television frame.
- Auto Console picks the preset that belongs to the game's console: a NES frame for NES games, a
  Game Boy shell for Game Boy games, and so on; a console with no art of its own falls back to
  None.
- Each preset has a landscape and a portrait layout: on a tall screen the frame and the picture stand
  at the top with the touch controls under them, so a portrait layout is its own art and picture
  rect, not the landscape one scaled.
- Presets are a setting like any other, so they live at every layer (Global, Libretro, console,
  core, game) and a game's own pick wins.

## Order

1. A preset catalog (data file, one entry per preset: shader preset path, aspect, and per
   orientation the overlay image's URL, size, checksum and the picture's rect in it) and its owner
   in `src/retro/`, next to the shader preset owner.
2. Overlay art is not bundled: a preset's images download when it is first chosen (or ahead, from
   Settings › Emulators like the shader pack), checked against the catalog's checksum and kept under
   `<data>/opensu/overlays/`; a preset whose art is missing runs without its frame and says so.
3. Overlay images drawn around the core's picture by the runner's compositor (the overlay pass
   already draws the touch layer's RGBA image; a bezel is the same pass under the picture's rect).
4. Auto Console: console key to preset, read from the same catalog.
5. Art: overlays need a licence that lets openSU download them; record each source in
   `docs/prior-art.md` before using it.
6. Proof: CRT on an arcade game shows scanlines and the TV frame; Auto Console on a NES game shows
   the NES frame, in landscape and in portrait.
