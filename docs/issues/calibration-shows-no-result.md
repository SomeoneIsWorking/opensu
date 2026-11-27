# Calibration ends without showing what it measured

Status: fixed (3fbc0258, d82c9c3b); shown on the Ally (2026-11-27).

Reported by the player (2026-11-26, after calibrating a controller in Devices › Controllers ›
Calibrate; the Ally's openSU was the one running): "OH OH OH, calibration, I did it but like there
is no preview screen or anything, I can't see if it worked fine".

Observed: the Calibrate card ends with a message naming any axis not swept far enough, and
nothing else. Expected: a screen after it that shows the sticks and triggers live, as corrected by
the new calibration (each stick's dot at the centre when let go, reaching the circle's edge, each
trigger reaching full), so the player can see it worked, keep it or calibrate again.

## Fix

Calibrating ends on a check screen (`PadCheckScreen`): both sticks as circles with the dot games
get and a faint mark for the raw reading, and both triggers as bars, live; Keep, Calibrate again,
and Undo back to the previous calibration. Devices › Controllers also has a Check row per pad that
opens the same screen without calibrating. On the Ally (1920x1080) the card first ran off the
bottom of the screen; the preview now takes only the height the card has left.
