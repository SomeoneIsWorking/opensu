# A libretro launch ends the moment it shows

Status: open. Not traced.

Observed in 1 of 20 hidden launches of Kirby & the Amazing Mirror on mGBA
(`scratch/sigterm-crash/runs/fixed-1/opensu.log`): `is running` and `shows a window` at
10:20:33.103, `stop opensu-…-game-0.scope` 6 ms later, `finished`, then `closed`. The runner logs
nothing at all, and `/state` never reports `inGame`. Expected: the game runs until it is closed.

The stop comes from `Handoff::start`'s phase two: `stillRunning` was false on its first check.
`instance_.running()` should still be true then, which leaves `ProcessTree::anyMatches`. A likely
cause, not yet confirmed: the first match was `systemd-run`, whose command line also holds the ROM
path, and it was read again while `systemd-run` was exec'ing into `opensu-retro`, when
`/proc/<pid>/cmdline` can read empty. To confirm, log the pids and command lines `matching` sees in
both phases and repeat the launch until it shows again.
