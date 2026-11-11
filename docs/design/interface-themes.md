# Interface themes

An interface theme is a whole console-menu feel chosen by one setting, Settings › Appearance ›
Interface theme: palette, type, shape, spacing, Home's layout options, the interface sounds and the
menu music. A theme is a folder of data, so a new one is a folder, not code.

Status: the format, the setting, the palette, type, shape and spacing are built. `tiles` and `home`
are read but drawn from the plan's step 3, `sounds` and `music` played from its step 4
(`docs/plans/channels-theme.md`).

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
| `ui::Shell::setTheme` | The face (`InterfaceText::useFace`), the HUD's ground and ink, the grid's tiles, the screens' theme |

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
| `tiles` | `style`: `glass` (iiSU's chrome and turning ring) or `channel` (a white glossy body, a thin edge, empty slots as wells, an accent outline on focus); `grow`, the focused tile's scale, 1 to 1.3; `body`, `edge`, `well`, `focus` colours |
| `home` | `scroll`: `scrolling` or `pages`, the scroll mode until the player picks one; `clockBar`: a band along the bottom edge with the time and the date; `pillClock`: the time in the top bar's pill |
| `sounds` | `set`: `marimba`, `glass` or `pluck`; `folder`: a folder in the theme with the effects' files (`assets/sounds/README.md` names them), each Ogg Vorbis or WAV up to 4 MiB; an effect it lacks plays `set`'s |
| `music` | An Ogg Vorbis or WAV loop in the folder, at most 16 MiB, played behind the menus when Settings › Audio › Menu music is on |

`palette` keys: the toolkit's `ground`, `surface`, `control`, `controlFocused`, `selected`, `ink`,
`inkMuted`, `inkFaint`, `accent`, `onAccent`, `track`, `focusRing`, `scrim`, `toast`,
`toastError`, `onToast`, `danger` (gpu2d `ui::Palette` says what each colours), and the shell's
`groundTop` and `groundBottom` (Home's ground, washed top to bottom), `groundDots` (iiSU's dot
grid, `true` or `false`) and `chromeInk` (the title pill, corner prompts and their glyphs).

Layout assumes a face whose capitals stand about 0.70 of the em (`ui::capHeightPerEm`), as Cal
Sans's and Nunito's do.

## What a theme cannot do

It is data: no scripts, no URLs, nothing fetched. Its files are opened only through the loaders
openSU uses for its own (FreeType through gpu2d's `Text` for a face, `audio::decodeSound` for
sounds), and only files inside its folder, links followed and checked.

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
  "home": { "scroll": "pages" },
  "sounds": { "set": "glass" }
}
```

## Checking it

`interface_theme_test` opens the shipped themes and refuses broken manifests;
`settings_controller_test` (`interfaceThemeIsChosen`) steps the row. In the running product, a
hidden openSU in its own home with `"interfaceTheme"` set in `settings.json` logs
`interface theme <name> from <folder>` and its frames show the theme.
