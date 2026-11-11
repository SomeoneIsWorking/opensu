# The Guide menu over Home fills the screen, its hints over its last row

Status: fixed.

Seen 2026-11-11 on origin/main 2e92f164 in a hidden openSU, 1280 x 800, demo library
(`tools.screenshots --parity --scenes home,guide`), on a desktop scaled 2.5 (`Xft.dpi` 240): the
Guide menu opened over Home is a full-width column of 100 px rows, Home's grid is hidden, and the
hint bar (B Close, A Select) is drawn over the Settings row. Expected, as the openSU build
installed on 2026-11-07 11:18 draws it: a left column of compact rows (Home, Library, Search,
Devices, Downloads, Settings, Power), the page's quick rows beside it, and Home showing to the
right.

Cause: `src/ui/shell.cpp` `Shell::dp` has taken the display's scale as its floor since 3534b2cf
(2026-11-07 14:43), and `src/app/shell_window.cpp` `ShellWindow::displayDp` gave it the desktop's
2.5 for a hidden window, so 1280 x 800 pixels were laid out as 512 x 320 dp instead of 1066 x 667
dp. Every toolkit screen was drawn 2.08 times larger; the Guide, narrower than its 572 dp
threshold, stacked as df4f651e made it do on a portrait screen. The Guide's layout commits
(df4f651e, 1eac798f, e744a4fd) are not the cause. Shown on one binary (a06c4f00): the frame as is
and with `SDL_VIDEO_X11_SCALING_FACTOR=2.5` are the same bytes; with `=1` the Guide is compact.

A second defect showed in that frame: a stacked row too short for its children
(`gpu2d` `Row::arrangeStacked`) gave the places list its whole height, so it ran under the hint bar
and the quick rows got none. That is what a phone in split-screen, or the longer in-game list on a
small phone, gets.

Fix:
- A hidden window is on no display. `ShellWindow::displayDp` returns the density the run states,
  `OPENSU_DISPLAY_SCALE` through `config` (1 unless told), which the runner's hidden window shares,
  and a hidden window is the size asked for with no minimum. `tools.screenshots` passes
  `OPENSU_WIDTH`, `OPENSU_HEIGHT` and `OPENSU_DISPLAY_SCALE` through, so a phone or tablet is drawn
  by naming it.
- gpu2d 83fd8ae: a stacked row too short for its children gives each the same share of its own
  height, so the places and the quick rows both stay on screen and scroll, clear of the hint bar.
- A window on a display now opens in dp: [window-opens-in-pixels.md](window-opens-in-pixels.md).

Shown 2026-11-11 on origin/main e5a4c6bf plus the fix, on the same desktop, frames looked at
(`OPENSU_WIDTH=.. OPENSU_HEIGHT=.. OPENSU_DISPLAY_SCALE=.. python -m tools.screenshots --parity DIR
--scenes home,guide,details`):
- 1280 x 800: the compact column, the quick rows beside it, Home on the right.
- Phone portrait, 1080 x 2400 at 2.625: the places above the quick rows, all on screen.
- Tablet portrait, 1600 x 2560 at 2: places and quick rows side by side, Home on the right.
- Phone landscape, 1600 x 720 at 2: side by side, both columns cut at the hint bar and scrolling.
- 1280 x 800 at 2.5 (the defect's own layout) and a phone in split-screen, 1080 x 1100 at 2.625:
  stacked, places and quick rows each part shown, the hint bar clear. Details stacks the same way.

Not walked: scrolling the shared lists to their last rows by pad in the short layouts.
