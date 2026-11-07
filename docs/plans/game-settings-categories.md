# Game settings by category

Requested 2026-11-07: "we can categorize options (game settings) better, I leave that up to you,
I think currently there is not categorizing". Then: "we need submenus in game settings because for
example FB Neo has too many core options".

## Now

A setting may name a section (`Setting::section`: Video, Resolution and upscaling, Frame
generation, Core options) and the editor starts a heading wherever the section changes in schema
order (`LayeredEditor`); settings that name none sit under no heading, and the order of the
schema, not a fixed list, decides how the sections fall.

Steps 4 and 5 are built: a core's v2 categories reach Game settings, each as a row ("Video, 6
settings") that opens a page of its options; options in no category stay on the main page. Seen
with mGBA's options at phone size; FinalBurn Neo not yet seen.

## What it does

Every setting names one category, and the editor shows each category once, under its heading, in
this order:

- Emulator: which emulator or core plays the game, its version.
- Video: shader preset and visual preset, aspect, scaling, resolution, frame generation.
- Audio: latency, volume.
- Controls: touch controls, mappings, turbo.
- Performance: run ahead, frame skip, rewind.
- Saves: save states, resume state, sync.
- Core options: one row per category the core declares (`retro_core_options_v2_category`: FinalBurn
  Neo's Dipswitches, Neo Geo, Audio, Frameskip and so on), each opening its own page of that
  category's options; options in no category stay on the Core options page itself. A core with
  only v0 or v1 options (no categories) keeps one page.
- A category is a submenu, not a heading: Game settings shows the categories as rows that open a
  page, so a core with a hundred options still opens on a short list.

## Order

1. One list of categories in `settings/setting_schema`; every setting names one, the current
   sections folded in (Resolution and upscaling and Frame generation under Video).
2. The editor groups rows by category in the list's order instead of following schema order
   (`app/layered_editor`).
3. Rows a caller adds (`EditorRows`) name their category too.
4. `retro/core_options` keeps each v2 option's `category_key` and the categories' descriptions, and
   `GET /options` reports them; the shell's core option rows carry the category.
5. The editor opens a category as a page over itself (Back returns to the list), the same page
   stack Settings uses.
6. Proof: the editor on a game, a console and Global shows the same categories in the same order,
   empty ones left out; FinalBurn Neo's Game settings opens on its categories, and Dipswitches
   lists only the game's dip switches.
