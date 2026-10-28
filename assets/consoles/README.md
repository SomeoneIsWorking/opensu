# Console art themes

Each folder here is one console art theme: the picture on a console's card in Library and the
small glyph in a framed tile's tab. Settings › Artwork › Console art theme picks one;
`src/artwork/console_themes.cpp` lists them and `src/ui/console_art.cpp` draws only from the chosen
one.

| Theme | Folder | What it is | Licence |
| --- | --- | --- | --- |
| Photos (default) | `photos/` | Photos of the hardware, public domain, CC BY or CC BY-SA as each is listed, with the Drawings glyphs | See [photos/README.md](photos/README.md) |
| Drawings | `drawings/` | openSU's own drawings of the hardware, no logos | MIT (`LICENSE`) |
| iiSU | none | iiSU's cards and glyphs, read from iiSU's release file the player chooses | iiSU's; not shipped |

## Layout

```
<theme>/
  theme.json
  cards/<system>.svg|webp|png|jpg
  glyphs/<system>.svg|webp|png|jpg
```

`<system>` is the ES-DE system name from `src/library/rom_systems.cpp` (`nes`, `snes`, `n64`, `gc`,
`wii`, `wiiu`, `switch`, `gb`, `gbc`, `gba`, `nds`, `n3ds`, `psx`, `ps2`, `ps3`, `ps4`, `psp`,
`dreamcast`, `xbox`, `xbox360`, `arcade`).

- An SVG is a mask: drawn white on a 64 x 64 box, openSU tints it to the card's colours, above
  the name and count.
- A WebP, PNG or JPEG is a picture, drawn in its own colours on the platform's gradient and filling
  the card: its longer side spans it, a twentieth or so running off each edge. The name and count
  are drawn over its foot in white with a black outline. Give it a transparent background, trimmed
  to the subject with a small margin; about 512 px on its longer side is plenty.

`theme.json`:

| Key | Meaning |
| --- | --- |
| `name` | Name shown in Settings; the folder name if left out. |
| `note` | One line shown under the setting: author, source, licence. |
| `cards` | `"tile"`: a card fills the whole tile and openSU draws no name or count over it. Leave it out for the name and count drawn with the picture. |
| `glyphs` | A shipped theme's folder name (such as `"drawings"`) whose glyphs this theme uses instead of its own `glyphs/`. |

## Your own theme

Put the folder in `$XDG_DATA_HOME/opensu/themes/<name>/` (`~/.local/share/opensu/themes/<name>/`
by default). It shows in Settings › Artwork › Console art theme once it has a
`theme.json` (`{}` is enough). Only the systems you have art for need files.

## No mixing

A theme shows only its own art. A system it has no card for shows a tile without a picture: the
platform's gradient, the name and the game count. A system it has no glyph for shows a tab
without one. openSU never fills a gap from another theme; iiSU's art has no PS4 card, so the PS4
tile in the iiSU theme has no picture.
