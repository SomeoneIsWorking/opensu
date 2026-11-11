# Channels theme: Home's grid, type and clock bar lay out wrong

Status: open, found 2026-11-11 in the proof frames of `docs/plans/channels-theme.md` step 3. Holds
the push of steps 2 to 4.

Seen in a hidden openSU with `"interfaceTheme": "channels"` (`tools.screenshots --parity
--interface-theme channels --scenes home,library,settings,settings-audio,console-shelf`, 1280 x
800). The iiSU theme's frames match the ones taken before the change, apart from the new rows.

## What is wrong

- Home and Library: the 4 x 3 page's tiles are about 90 px, the page sits to the left, and the
  right half of the screen is empty. Expected: the page fills the width above the clock bar, the
  same as the Wii-inspired target in the plan.
- Tile labels overlap: "Game Boy Color" and "1 game" are drawn over each other in the small cells.
- Settings rows: the label sits about 20 px lower than under the iiSU theme while the note stays
  put, so the two overlap ("UI sounds" over "The sounds menus and launches make"). The title pill's
  text sits low too.
- The clock bar takes the bottom third of the screen (top at y 525 of 800): `Hud::bottomInset`
  stacks the 50 dp clock row on top of iiSU's prompt-row inset, which the dock already fills.
- The status pill keeps its divider with nothing after it once `pillClock` is false.

Done in the tree, not yet proven in frames: the stored Nunito's descents (below), and
`StatusPillPainter::paintLaunchers` leaving out the divider when the text row is empty. Open: the
grid and the clock bar.

## Leads

- Grid, the cause: `ui::HomeLayout`'s Paged layout is iiSU's WiiSu mode. It sizes square cells
  from the height left between the insets and `rows`, and `wiiSuPageColumns` then takes as many
  columns as fit across, about 11 for 90 px cells. Cells fill column-major (`cellOf`), so ten
  tiles take the first four columns of an 11-column page, which leaves the right half empty. The
  plan's "4 x 3 cells a page" was never expressed: the theme has no tokens for the page's columns
  and rows, the cells' aspect or the fill order. The fix is to add them to `home` and `tiles`
  (`columns`, `rows`, `aspect`, row-major fill), have `HomeLayout` take them in Paged mode (columns
  fixed, cells sized by the tighter of width and height, the page centred), and have
  `TilePainter`'s channel style fit its art into a wide cell. iiSU's defaults keep today's
  layout.
- Type, the likely cause: the shipped `Nunito-Bold.ttf` (vernnobile/NunitoFont 2.0) sets
  `USE_TYPO_METRICS` (fsSelection 0xa0) while its `sTypoDescender` is +572, positive, and its
  `usWinDescent` reads 64964, a negative stored unsigned. FreeType then takes the typo metrics, so
  gpu2d's `Text::metrics` (`TTF_GetFontAscent`/`TTF_GetFontDescent`) gives a line box of about
  0.70 em where Cal Sans gives 1.30 (em 1000, ascent 1000, descent -300, cap 700). Every layout
  built on `lineHeight` packs Nunito's lines too close. Fix it at the data: a Nunito release with
  sound vertical metrics (the googlefonts/nunito build, OFL), checked by reading its OS/2 and
  hhea tables before it is committed. `theme::parseTheme` could refuse a face whose typo descender
  is positive, so a player's theme gets a reason instead of broken text.
- Clock bar: the band should share the prompt row's height rather than add to it, or the grid's
  rows go down to two under it.

## Checking it

The same capture with `--interface-theme channels`. Look at the frames: Home's page fills the width,
no text overlaps, and the bar is about the dock's height.
