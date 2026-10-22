# A libretro launch ends the moment it shows

Status: fixed, not committed.

Observed in 1 of 20 hidden launches of Kirby & the Amazing Mirror on mGBA
(`scratch/sigterm-crash/runs/fixed-1/opensu.log`): `is running` and `shows a window` at
10:20:33.103, `stop opensu-…-game-0.scope` 6 ms later, `finished`, then `closed`. The runner logs
nothing at all, and `/state` never reports `inGame`. Expected: the game runs until it is closed.

## Reproduction

`scratch/launch-early/tools/loop.py BUILD RUN COUNT [natural]` (with `uv run --frozen python -I`):
one hidden shell in scratch dirs launches Kirby on the built-in mGBA COUNT times and ends each
launch with Close game (or, with `natural`, SIGTERM to the runner's PID). A launch whose
`finished` comes before the close is an early end.

The natural rate is too low to reproduce on demand (0 of 80 in one shell, 0 of 100 in fresh
shells, also under manager and CPU load): it needs `systemd-run` to be read before its exec, which
only happens when the user manager is slow (941 ms to join the scope in `fixed-1`, about 20 ms
normally). The `scratch/launch-early/shim` stand-in widens the same moment: `PATH` points at a
`systemd-run` that puts a delayer between `--` and the program; the delayer carries the program's
argv and execs itself `LAUNCH_EARLY_EXECS` times before execing the program, as `systemd-run` does
once.

## Cause

`src/launch/handoff.cpp:Handoff::stillRunning` took "no command line contains the hint"
(`ProcessTree::matching`) as the game gone, but `/proc/<pid>/cmdline` of the launch's own started
process reads empty while that process execs (`systemd-run` into `opensu-retro`), so a first check
that lands in the exec ends a game that is still starting.

Seen with logging of each check (`runs/chain-base-1/opensu.log`): phase one matched the started
pid, phase two's first check found `instance true matches 0`, and the same pid, state `R`, read
`cmdlen=0` at that moment and the full command line a moment later. A standalone probe of
`systemd-run --scope` shows 5–9 empty reads per exec on a live pid whose start time is unchanged.
Phase one had the same hole: inside Gamescope, the same read failed a launch as `closed before it
showed a window`.

## Fix

`ProcessLauncher::startedRunning`: the started process has not exited (`waitpid`, the pid it keeps
through every exec). A game in a scope is running while that holds, then while the scope is active
and a process matches the hint (`Handoff::stillRunning`); phase one only reports a game that
appeared and then vanished as closed once the started process has exited too
(`Handoff::awaitScopeWindow`). A process other than the started one that execs while matching the
hint, after the started one has exited, can still read empty for that moment.

## Proof

- Before (5603e2e plus logging, `build/agent-early`): `runs/chain-base-1` 3 of 10 early ends,
  `runs/chain-base-2` 6 of 30.
- After (`build/agent-early-fix`): `runs/chain-fix-1` 0 of 100 under the shim;
  `runs/plain-fix-1` 0 of 100 without it, each Close game over in 0.02–0.14 s;
  `runs/natural-fix-1` 20 of 20 runner exits seen by the shell in 0.16–0.25 s, each toast
  `Kirby & the Amazing Mirror closed`.
