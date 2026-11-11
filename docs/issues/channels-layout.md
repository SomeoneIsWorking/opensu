# Channels theme: Home's grid, type and clock bar lay out wrong

Status: fixed 2026-11-11. Found the same day in the proof frames of `docs/plans/channels-theme.md`
step 3 (`tools.screenshots --parity --interface-theme channels`, 1280 x 800).

## What was wrong, and why

- Grid: ten tiles sat about 90 px wide in the top-left with the right half of the screen empty.
  `ui::HomeLayout`'s Paged layout was only iiSU's WiiSu page: square cells sized from the height,
  `wiiSuPageColumns` taking as many columns as fit (about 11), filled column by column
  (`cellOf`). The plan's 4 x 3 page had no tokens and no layout.
- Clock bar: it took the bottom third of the screen (top at y 525 of 800) because
  `Hud::bottomInset` added a 50 dp clock row to iiSU's prompt-row inset, which the dock already
  fills.
- Type: labels overlapped and sat low. The shipped `Nunito-Bold.ttf` set `USE_TYPO_METRICS` with a
  positive `sTypoDescender`, so its line box was about 0.70 em where Cal Sans gives 1.30. Fixed in
  4e2f15a6 with the stored font's descents corrected.
- The status pill kept its divider with nothing after it once `pillClock` is false. Fixed in
  4e2f15a6 (`StatusPillPainter::paintLaunchers`).

## The fix

- The page is tokens (`docs/design/interface-themes.md`, "The page"): `home.columns`, `home.rows`,
  `home.fill` and `tiles.aspect`. Channels sets 4, 3, `rows` and 1.5. `HomeLayout` lays such a set
  page out (`SetPage`, `layoutSetPage`): cells as large as fit between the page arrows and the
  bars, the page centred, one page to a screen, the page pill under it, a tall screen taking three
  across and up to six rows. iiSU's page is untouched without the tokens.
- `HomeGrid` draws the page's unfilled cells as wells (`HomeLayout::emptyCells`), slides the page
  when it turns, and turns it when Left or Right leaves its edge towards a page whose row is
  empty.
- `TilePainter`'s `channel` style crops art to the wide cell while three quarters of it stay, and
  otherwise stands it whole over a faint crop of itself (`artBox`); a game without art reads its
  name instead of iiSU's pale letter.
- The clock bar is as tall as the dock's row: the time stands before the dock and the date after
  it (`Hud::drawClockBar` from `layoutDock`). Under 600 dp of width they share a 26 dp line above
  the dock's row, the only case that adds to the grid's bottom inset.

## Proof

Hidden openSU on origin/main b8b3b278 plus the fix, `SDL_VIDEO_X11_SCALING_FACTOR=1`, frames under
`scratch/channels-layout/`:
- 1280 x 800 (`channels-1280x800/`): Home's ten tiles and two wells fill the width; Library's
  twelve; a 30-game console shelf on three pages; Right from the last column lands on the next
  page's first tile in the same row, with the page mid-slide in `shelf-turning.png`; Left comes
  back; Right from the bottom row onto the six-tile last page lands in its last row. The band is
  86 px of 800 with the time left of the dock and the date right of it.
- Portrait 960 x 1280 and 960 x 1560 at 2 px per dp (`channels-portrait-*`): three across, five or
  six rows; at 480 dp the time and date stand on one line above the dock.
- The iiSU look: `before-iisu/` (origin/main) and `after-iisu/` differ only in the status pill's
  clock digits and the focused tile's turning ring (home, library, console-shelf, settings,
  details).
- `home_layout`, `grid_focus`, `home_screen_flow` and `interface_theme` tests cover the page, its
  window and wells, focus across pages and the tokens.

## Left

- Not seen on a phone or tablet, and the pointer and touch not walked by hand. A hidden window is
  at least 960 px wide, so a 720 px phone's width was stood in for by 960 px at 2 px per dp.
- The page pill and arrows keep iiSU's glass look.
