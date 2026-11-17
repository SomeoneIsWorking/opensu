# Touch controls over remote play ignore the player's controls

Status: fixed 2026-11-17; checked on Linux by the focused tests, not yet on the Mac or a tablet.

Reported 2026-11-17 by the player, relayed by the macOS session: on a remote play guest the touch
controls appear even with a controller connected, or with Touch controls set to Never. On the Mac
they appear over every remote game.

## Cause

- `ShellApp::handlePointer` showed them whenever `ShellWindow::hasTouchscreen` and no pad was
  connected; it never read the game's Touch controls setting (`settings::TouchShow`), and a
  controller connected but not yet pressed counted as none.
- `ShellWindow::hasTouchscreen` counted every SDL touch device. SDL lists a Mac's trackpad as one
  (`SDL_TOUCH_DEVICE_INDIRECT_*`), so a Mac always had a "touchscreen".
- A touch over a remote game was never noted as the latest input (`ShellApp::pointer` returned
  first), so Dynamic could not bring the controls back after a controller's press.

## Fix

- The rule is one function, `settings::touchWanted` beside `TouchShow` and
  `settings::resolveTouchShow`; the runner and remote play both call it.
- Remote play reads the remote game's Touch controls and Touch controls size through its setting
  layers each time the game gets the controls back, so a change in the Guide applies.
- Only a direct touch device (a touchscreen) counts in `hasTouchscreen`.
- A pointer over a remote game is noted (`LastDevice::notePointer`), so a touch brings Dynamic's
  controls back.
