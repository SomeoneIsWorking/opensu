# A window on a scaled desktop opens too small for its layouts

Status: partial. Fixed and unit-tested through the shipping rule; not yet seen on a visible window.

Found 2026-11-11 while tracing [guide-over-home-full-width.md](guide-over-home-full-width.md):
openSU's own window opened at `OPENSU_WIDTH` x `OPENSU_HEIGHT` (1280 x 800) in SDL's window units,
which are pixels on X11 and Windows. On a desktop scaled 2.5 (`Xft.dpi` 240) that window is
512 x 320 dp, and since 3534b2cf the interface is never drawn smaller than the display's dp
(`Shell::dp`), so every toolkit screen is crowded: the Guide stacks, and Details and Settings lay
out for a width narrower than a phone's. The player inside openSU's own Gamescope does not see it
(the scale there is 1 and the window fills the output); a run without Gamescope on Linux and every
windowed run on Windows does.

Cause: `src/app/shell_window.cpp` `ShellWindow::ShellWindow` handed the configured size and the
960 x 600 minimum to SDL as they were, in window units instead of dp.

Fix: a window on a display is sized in dp. `display::windowOn` multiplies the size by the display's
content scale (1 where the system already sizes windows in points, as macOS and Wayland do) and
keeps it inside the display's usable area; `ShellWindow` opens at that size, centred, with the
minimum scaled the same way and never larger than the window, and logs
`window of 1280x800 dp opens WxH on a display of S per dp`. A hidden window keeps its size in
pixels, at the density `OPENSU_DISPLAY_SCALE` states. Since then a dp is the display's scale times Interface size (`display::DisplayScale`, docs/design/sdl3-gpu-shell.md 4.1), with no fit to the window.

Shown: `ctest -R window_size` (1 per dp opens 1280 x 800; 2.5 per dp opens 3200 x 2000 with a
2400 x 1500 minimum; a 1920 x 1040 usable area at 2 per dp holds it to that).

Left: a visible window cannot be opened by an agent without taking the player's focus and pads. On
a scaled desktop, outside Gamescope, the player checks that the window opens at the scaled size,
centred, and that the log line above names it.
