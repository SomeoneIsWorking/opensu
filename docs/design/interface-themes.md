# Interface themes

An interface theme is a whole console-menu feel chosen by one setting, Settings › Appearance ›
Interface theme: palette, type, shape, spacing, Home's layout options, the interface sounds and the
menu music. A theme is a folder of data, so a new one is a folder, not code. Choosing a theme
gives what it sets back to it: the scroll mode the player had picked (Settings › Appearance) and
the interface sounds (Settings › Audio, back to Theme's) go back to the theme's.

Status: built, every key below; `docs/plans/channels-theme.md` is the plan it came from.

## Where themes live

- Shipped: `assets/interface-themes/<id>/`. `iisu` is the default; `channels` is the second
  (`docs/plans/channels-theme.md`).
- The player's: `<data dir>/interface-themes/<folder>/` (on Linux
  `~/.local/share/opensu/interface-themes/`), listed after the shipped ones with the id
  `user/<folder>`.
- A folder counts when it holds `theme.json`. The folder rule is `theme::ThemeFolders`, which the
  console art themes (`<data dir>/themes/`) follow too: an id with a path in it names nothing.

## Owners

| Owner | Does |
| --- | --- |
| `src/theme/interface_theme.*` | The tokens with their defaults (`InterfaceTheme`) and the strict reader (`parseTheme`, `readTheme`) |
| `src/theme/interface_themes.*` | The themes there are (`list`), one by id (`open`), or the default when it does not open (`openOrDefault`, logged) |
| `src/theme/theme_folders.*` | Shipped and player folders, ids, `user/` |
| `src/app/interface_theme_settings.*` | The setting's row, and `current`: the one theme in force, which `ShellApp::applyLayout` hands to the shell |
| `src/ui/interface_theme.*` | The tokens as the toolkit's `gpu2d::ui::Theme` |
| `ui::Shell::setTheme` | The face (`InterfaceText::useFace`), the HUD's ground, ink and clock bar, the grid's tiles (`TilePainter::setTiles`, the focus grow) and page (`HomeLayoutInput::page`), the screens' theme |
| `src/ui/home_layout.*` | A page of set columns (`SetPage`): its cells' size, the fill order, the wells (`emptyCells`) |
| `InterfaceThemeSettings::homeMode` | The scroll mode: the player's, else `OPENSU_HOME_MODE`'s, else the theme's `home.scroll` |
| `src/app/interface_sounds.*` | The interface sounds: the player's choice, else the theme's `sounds.set` with its folder's files over it (`theme::soundFile`) |
| `src/app/menu_music.*` | The theme's `music`, looped while Settings › Audio › Menu music and the UI sounds are on and no game runs |

## `theme.json`

UTF-8 JSON, at most 64 KiB. Every key below is optional but `format` and `name`; a key the format
does not know, a value of the wrong type or out of its range, a colour that is not `#RRGGBB` or
`#RRGGBBAA`, or a path that is not inside the folder refuses the whole theme, with why in the log
(`theme` tag), and the shell stays on (or falls back to) the default. A theme sets only what it
changes; everything else keeps the default, which is iiSU's look.

| Key | Value |
| --- | --- |
| `format` | `1` |
| `name` | What the setting shows, at most 48 bytes |
| `author`, `note` | Who made it; one line for the row: whose and its licence (at most 160 bytes) |
| `palette` | Colours, below |
| `type` | `face` (a `.ttf` or `.otf` in the folder, at most 16 MiB; openSU's Cal Sans when unset), and `title`, `heading`, `body`, `caption`, `value`, `hint`, `toast` in dp, 6 to 64 |
| `shape` | `radius`, `smallRadius` (0 to 40 dp), `outline`, `focusRing`, `focusGap` (0 to 8 dp) |
| `spacing` | `gap`, `pad`, `section` (0 to 64), `rowHeight`, `compactRowHeight` (to 160), `sidebarWidth`, `contentWidth`, `dialogWidth` (to 1200), `hintBarHeight` (20 to 120), in dp |
| `tiles` | `style`: `glass` (iiSU's chrome and turning ring) or `channel` (a white glossy body, a thin edge, empty slots as wells, an accent outline on focus); `grow`, the focused tile's scale, 1 to 1.3; `body`, `edge`, `well`, `focus` colours; `aspect`, a tile's width over its height on a page of set columns, 1 to 2 |
| `home` | `scroll`: `scrolling` or `pages`, the scroll mode until the player picks one; `columns` (1 to 8), `rows` (1 to 6, 3 unset) and `fill` (`rows` or `columns`), the page of pages mode, below; `clockBar`: a band along the bottom edge the dock stands on, with the time and the date; `pillClock`: the time in the top bar's pill |
| `sounds` | `set`: `marimba`, `glass` or `pluck`; `folder`: a folder in the theme with the effects' files (`assets/sounds/README.md` names them), each Ogg Vorbis or WAV up to 4 MiB; an effect it lacks plays `set`'s |
| `music` | An Ogg Vorbis or WAV loop in the folder, at most 16 MiB, played behind the menus when Settings › Audio › Menu music is on |

`palette` keys: the toolkit's `ground`, `surface`, `control`, `controlFocused`, `selected`, `ink`,
`inkMuted`, `inkFaint`, `accent`, `onAccent`, `track`, `focusRing`, `scrim`, `toast`,
`toastError`, `onToast`, `danger` (gpu2d `ui::Palette` says what each colours), and the shell's
`groundTop` and `groundBottom` (Home's ground, washed top to bottom), `groundDots` (iiSU's dot
grid, `true` or `false`) and `chromeInk` (the title pill, corner prompts and their glyphs).

### The page

Without `home.columns` a page of pages mode is iiSU's: as many square tiles across as fit, filled
column by column, the next page peeking in. With it the page is set: `columns` by `rows` tiles of
`tiles.aspect`, as large as fit between the page arrows and the bars, centred, one page to a
screen. It fills in `fill` order, its cells past the last tile are drawn as empty slots (wells in
the `channel` style), a turn slides the page, and Left or Right off its edge turns to the next
page even where that page's row is empty. A screen taller than wide takes the lesser of `columns`
and `rows` across and as many rows as fit, up to 6. A screen with room for more shows more
tiles, not larger ones: a column joins while a cell would pass 360 dp across, up to 8, then the
rows that fit. The icon size (Settings › Appearance) does not
change a set page. In the `channel` style a game's art is cropped to the cell while the crop
keeps three quarters of it, and otherwise stands whole on the tile's body.

### The clock bar

`clockBar` draws a band as tall as the dock's row along the bottom edge. On a screen at least
600 dp wide the time stands before the dock and the date after it; on a narrower one both share a
line above the dock's row, which takes 26 dp more from the grid.

Layout assumes a face whose capitals stand about 0.70 of the em (`ui::capHeightPerEm`), as Cal
Sans's and Nunito's do.

## What a theme cannot do

It is data: no scripts, no URLs, nothing fetched. Its files are opened only through the loaders
openSU uses for its own (FreeType through gpu2d's `Text` for a face, `audio::decodeSound` for
sounds, which refuses more than `audio::decodedLimit` of decoded sound), and only files inside its
folder, links followed and checked: the sounds folder's files each time they are loaded.

## A minimal theme

`<data dir>/interface-themes/dusk/theme.json`:

```json
{
  "format": 1,
  "name": "Dusk",
  "author": "You",
  "note": "Your own theme",
  "palette": {
    "groundTop": "#2B2733",
    "groundBottom": "#1B1726",
    "groundDots": false,
    "chromeInk": "#E6E4EE"
  },
  "tiles": { "aspect": 1.5 },
  "home": { "scroll": "pages", "columns": 4, "rows": 3, "fill": "rows" },
  "sounds": { "set": "glass" }
}
```

## Checking it

`interface_theme_test` opens the shipped themes, refuses broken manifests and sound files outside
the sounds folder; `settings_controller_test` (`interfaceThemeIsChosen`, `interfaceSoundsAreChosen`)
steps the rows. In the running product, a hidden openSU in its own home with `"interfaceTheme"`
set in `settings.json` logs `interface theme <name> from <folder>` and its frames show the theme;
with the theme's sounds it logs `<n> of 12 UI sounds are <name>'s`, and with `--sound` and
`"menuMusic": true`, `menu music <name>: <seconds> s from <file>` and `menu music plays`
(`SDL_AUDIO_DRIVER=disk` writes what the device plays to a file).
