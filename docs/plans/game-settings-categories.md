# Game settings by category

Requested 2026-11-07: "we can categorize options (game settings) better, I leave that up to you,
I think currently there is not categorizing".

## Now

A setting may name a section (`Setting::section`: Video, Resolution and upscaling, Frame
generation, Core options) and the editor starts a heading wherever the section changes in schema
order (`LayeredEditor`); settings that name none sit under no heading, and the order of the
schema, not a fixed list, decides how the sections fall.

## What it does

Every setting names one category, and the editor shows each category once, under its heading, in
this order:

- Emulator: which emulator or core plays the game, its version.
- Video: shader preset and visual preset, aspect, scaling, resolution, frame generation.
- Audio: latency, volume.
- Controls: touch controls, mappings, turbo.
- Performance: run ahead, frame skip, rewind.
- Saves: save states, resume state, sync.

## Order

1. One list of categories in `settings/setting_schema`; every setting names one, the current
   sections folded in (Resolution and upscaling and Frame generation under Video).
2. The editor groups rows by category in the list's order instead of following schema order
   (`app/layered_editor`).
3. Rows a caller adds (`EditorRows`) name their category too.
4. Proof: the editor on a game, a console and Global shows the same categories in the same order,
   empty ones left out.
