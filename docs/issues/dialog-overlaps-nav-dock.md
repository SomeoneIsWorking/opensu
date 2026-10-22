# The first-run integrations dialog sits over the nav dock

Status: open.

Observed (v0.1.0 AppImage smoke, `build/appimage/smoke/ubuntu-24.04.png`, 1280x800): the "Choose
your integrations" dialog ends at y 712 and the nav dock's LB/RB caps and icons show beneath its
bottom edge. Expected: nothing of the dock under a dialog, as with the docked keyboard
(`ui::Shell::dockedKeyboard` hides the dock).

To check: which painter owns the dialog's frame, and whether the dock should hide under every
modal (one rule in `ui::Shell`) or the dialog should keep clear of the dock.
