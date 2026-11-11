# Interface themes, and Channels, the second one

Asked for 2026-11-11 (G002): openSU's look, layout and sound come as themes, one "Interface
theme" setting choosing a whole console-menu feel. The iiSU look stays the default; the second is
Channels, a light, pointer-first look in the spirit of the Wii's menu. A theme is a folder of data
(`docs/design/interface-themes.md` is its format), so a third theme is one more folder, shipped or
the player's, and no new code path.

## Target

- Settings › Appearance › Interface theme steps through the themes there are: the shipped
  `assets/interface-themes/iisu/` (the default) and `channels/`, then each folder in
  `<data dir>/interface-themes/`. Choosing one changes palette, type, shape, spacing, Home's layout
  options, the interface sounds and the menu music together.
- A theme sets the defaults of the choices the player already makes on their own: the scroll mode
  (Settings › Appearance) and the interface sounds (Settings › Audio). Those rows gain "Theme's"
  as their first entry, which follows the theme. Choosing a theme puts both back to "Theme's", so a
  theme always arrives whole; the player may override either after.
- Menu music: a calm loop the theme may ship, played on Home and the menus while no game runs.
  Off by default, behind its own switch (Settings › Audio › Menu music).
- One owner of the current theme: `theme::InterfaceThemes` lists and opens the folders and
  `ShellApp::applyLayout` hands the opened theme to the shell, the sounds and the music; nothing
  else reads the setting.

## What is taken from the Wii Menu, and what is not

Read from screenshots and articles only (Wikipedia's "Wii Menu" and "Wii system software"): the
menu is four pages of a 4 x 3 grid of channels, each a rounded live tile, selected by the
remote's pointer, with the time and date on every page.

Taken, as a design language:
- pages of rounded tiles, four across and three down, with empty slots drawn as faint wells;
- a soft white and light-grey look, glossy tiles with a thin grey edge, pale blue accents;
- rounded type;
- a bottom bar carrying a large clock and the date under it;
- a gentle grow under the pointer or focus, a soft press dip;
- a calm ambient loop and soft tick and chime interface sounds;
- pointer-first: the grid is laid out for pointing, the pad and keys still work.

Not taken: any of Nintendo's art, icons, fonts, sounds or music, the name or logo, the bar's
buttons or exact curve, or a pixel-level recreation. The theme is called Channels; "Wii-inspired"
appears only in docs. Its type is Nunito Bold (SIL OFL 1.1, from the font's own repository,
`vernnobile/NunitoFont`, version 2.0), stored with its licence like Cal Sans. Its sounds and music
are synthesised by `tools/sounds/`, openSU's own, MIT.

## Tokens

Every token has a default, the iiSU look's value, so a theme states only what it changes; the
format doc lists them with ranges. Groups:

- `palette`: the toolkit's colours (ground, surface, control, selected, ink, inkMuted, inkFaint,
  accent, onAccent, track, focusRing, scrim, toast, toastError, onToast, danger) and the shell's
  (`groundTop` and `groundBottom` for a vertical wash, `groundDots` for iiSU's dot grid, the title
  ink, the tile colours below).
- `type`: the face file (a TTF or OTF in the folder; Cal Sans when unset) and the toolkit's sizes.
- `shape`: radius, small radius, outline, focus ring and gap.
- `spacing`: the toolkit's lengths.
- `tiles`: `style` (`glass`, iiSU's chrome and turning cyan-violet ring; `channel`, a white glossy
  body with a thin edge and an accent outline when focused), the focus grow (scale), the edge,
  body and well colours.
- `home`: the scroll mode it defaults to (`scrolling` or `pages`), whether Home has a bottom bar
  with the clock and date (`clockBar`), and whether the top bar's pill still shows the clock.
- `sounds`: a shipped set by name (`marimba`, `glass`, `pluck`) or a folder in the theme holding
  the effect files (`src/audio/effect.hpp`'s names); a missing file plays the default set's.
- `music`: an optional Ogg Vorbis or WAV loop in the folder.

## Layout under Channels

All of it is tokens and options on the existing owners, not a second Home:
- `HomeGrid` in Paged mode (the theme's scroll default) with 4 x 3 cells a page; empty slots as
  wells (`TilePainter`, `channel` style).
- The bottom bar (`Hud`): a pale band along the bottom edge with a soft top rim, the time large in
  its middle and the date under it, from `TopBarClock`'s tick (`ui::ClockText` and a date line);
  the grid's bottom inset grows by the band. The dock stands on the band.
- Hover-grow: the pointer focuses the tile under it (`CellGrid::hover`, as today) and the focused
  tile eases up to the theme's grow; the press pulse dips it.

## Sounds under Channels

Folder `assets/interface-themes/channels/sounds/`, one file per effect, from a new timbre in
`tools/sounds/timbres.py` on the same score: a soft sine-and-octave "tick" bell (two near-harmonic
partials, a fast decay) in D major pentatonic, a short airy room.

- `navigation.ogg`: one short tick, the fifth;
- `open.ogg`, `close.ogg`: a two-note chime up, and down;
- `context-menu.ogg`: a quick third-and-sixth strum;
- `enter.ogg`, `exit.ogg`: a rising and falling four-note run;
- `launch.ogg`: a run into a ringing chord;
- `section-*.ogg`: one to five light taps.

`music.ogg`: about 32 s, seamless, slow (72 bpm) soft electric-piano chords over a pad in the same
key, at -30 LUFS integrated so it sits under the cues; the loop's end crossfades into its start.

## Steps, each its own commit

1. This plan.
2. The format and the setting: `docs/design/interface-themes.md`; `theme::InterfaceTheme` (tokens
   with defaults) and `theme::parseTheme` (strict: bounded file, known keys, ranges, paths kept in
   the folder); `theme::InterfaceThemes` (the shipped and the player's folders by the one folder
   rule the console art themes use); the iiSU theme as `assets/interface-themes/iisu/`; the
   setting (`settings::Settings::interfaceTheme`) and its row; the toolkit theme, the ground and
   the face from the opened theme; tests over the shipped themes and malformed ones.
3. The layout pieces: the `channel` tile style and grow, the clock bar, the scroll default; the
   Channels folder with its palette and Nunito.
4. The sound set and the menu music: the timbre, the cues and loop, the Interface sounds row's
   "Theme's", the music switch and its player.

## Proof

Hidden openSU in a home of its own under `scratch/channels-theme/`: Home, a console's shelf,
Settings and a game's details in both themes; the iiSU frames compared with frames taken on HEAD
before the change. The cues and the loop are files in the theme folder, played back to check.

## Left out

- Wii Remote and DolphinBar pointer input: a later task.
- The painters that still draw with the fixed `palette` (on-screen keyboard, mapping editor, chord
  list, volume display) keep the iiSU colours until they take the theme too.
