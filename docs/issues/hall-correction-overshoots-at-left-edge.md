# Hall-effect correction pulls LS right when it is all the way left and LT is pulled

Status: fixed in the commit that adds this section; checked on the Ally's Check screen.

Reported by the player (2026-11-27, ROG Ally): "hall effect correction works but it over applies
when LS is all the way to the left and LT is pulled, it pulls LS slightly to the right, if you
read the inputs now, you can see it".

Observed: LS held fully left with LT pulled reads a little right of full left after the
correction. Expected: full left stays full left whatever LT does.

## Cause

The Ally's pad reported `ABS_X -32768` (its end) with `ABS_Z 255` while held so.
`gamepad/hall_correction.cpp:corrected` took the trigger's pull out as a plain shift
(`- triggerPullX * trigger`, +0.28 at a full pull by default), but a reading the pull moves toward
an end stops there, so a stick at the left end came out at about -0.72.

## Fix

`withoutPull` shifts only the side the pull moves away from; the side it moves toward, which the
end cuts short, is stretched from the shifted centre back to the end. The rest offset is taken out
after it, as before. `pad_translator_test` checks a full left with the trigger pulled.
