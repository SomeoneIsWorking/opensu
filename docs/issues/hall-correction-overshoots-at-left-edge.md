# Hall-effect correction pulls LS right when it is all the way left and LT is pulled

Status: fixed; see Fix and Measurement.

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

`withoutPull` takes the pull out as a plain shift, except over the last two pulls of travel toward
the end the pull moves to: there the reading is stretched by 1.5 back to the end, so the end stays
the end and the stretch meets the shift where it starts. `pad_translator_test` checks a full left
and the measured point below.

## Measurement (2026-11-27, Ally, calibrated pull -0.12, Stick correction Off)

With the player holding the left stick still and moving only the left trigger, `ABS_X` went from
-17430 (-0.532) at `ABS_Z` 0 to -21729 (-0.663) at 255: a pull of -0.131 there, against the
calibration's -0.12 at the right end. The pull is about the same across the travel, so the first
fix (2f87b56d), which faded it over the whole left half, took out too little there (-0.617 for
-0.532); this one gives -0.543.
The player (2026-11-27, ROG Ally): "I think the effect can scale with how far it is away from fully
pulled from the left side". It fades to nothing at the left end, over the last two pulls of travel
(the measurement above showed the pull whole at -0.53).
