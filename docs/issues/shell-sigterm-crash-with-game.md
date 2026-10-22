# Shell crashes on SIGTERM while a game runs

Status: fixed in the tree, not committed.

Observed: SIGTERM to a hidden `opensu` while a libretro game runs ends it with SIGSEGV instead of a
clean exit. Expected: the shell closes the game and exits 0.

Reproduction: `scratch/sigterm-crash/tools/repro.py BUILD RUN COUNT [--gdb]` (run with
`uv run --frozen python -I`, `PLAY=0.6`): scratch XDG dirs, hidden shell, Kirby & the Amazing
Mirror on the built-in mGBA through the control channel, SIGTERM to the shell's PID 0.6 s after
`inGame`.

## Cause

`src/app/game_launch.cpp:GameLaunch::start` detached the launch thread, so nothing joined it when
`ShellApp` was destroyed: `~ShellApp` destroyed `handoff_` (whose `ProcessLauncher` stopped the
scope) while that thread was still in `Handoff::stillRunning`, which then read the freed
`ProcessLauncher::group_`.

Backtrace (`scratch/sigterm-crash/runs/base-gdb-1/opensu.log`): the launch thread faults at
`ProcessLauncher::running` (`group_->unit.empty()`) ← `Handoff::stillRunning` ← `Handoff::start` ←
the `GameLaunch::start` lambda, with `this` on the main thread's stack; the main thread is already
past `handoff_` in `~ShellApp`, joining `ArtworkFetcher`'s thread. The log has
`stop opensu-…-game-0.scope` from `~ProcessLauncher` and never the launch's `is over` line.

## Fix

`GameLaunch` owns the thread (`std::jthread thread_`, its last member). Its destructor calls
`Handoff::forceClose` when a launch thread exists, and the member's destructor joins it, so the
game ends through the usual close path (scope stopped with SIGTERM, the runner writes its save, the
end told to the mailbox) before `ShellApp` destroys anything that thread uses. `GameLaunch` is
declared after `handoff_`, `mailbox_` and `retro_`, so it goes first. `start` joins the previous
launch's thread, which has by then only its end notice left to post.

## Proof

- Before (`build/agent-crash` at cd7737a): `runs/base-summary.txt`, 5 of 6 SIGSEGV (exit -11);
  `runs/base-gdb-1`, `base-gdb-2` under gdb, 2 of 2.
- After: `runs/fixed-summary.txt`, 20 runs, 0 crashes; each of the 19 that reached the game exited
  0 within 0.2–2.6 s, logged `[retro] ran N frames` and
  `… is over: Kirby & the Amazing Mirror closed`, its `.srm` written after the SIGTERM, no runner
  left. `fixed-1` never got that far: the launch ended 6 ms after it showed a window, before any
  SIGTERM (`launch-ends-as-it-starts.md`).
