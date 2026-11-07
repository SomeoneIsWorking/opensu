# A game keeps running after Android kills the shell, and the Guide cannot hold it

Status: fixed, not yet seen on the phone.

Observed 2026-11-07 on the phone (VKJ-NX9, build `0.6.0-88-gdf4f651e`): the Guide's menu over Alien
vs. Predator left the game running behind it.

## Evidence

`files/opensu.log` and `dumpsys activity exit-info`:

- 17:34:00 (local) the shell (pid 1936) starts the game in `RetroActivity` (`:retro`, pid 6667),
  control file `cache/run/retro-1936-0.json`.
- 17:34:08 the system kills the shell: `DEPENDENCY DIED`, "depends on provider
  com.android.externalstorage/.ExternalStorageProvider in dying proc". The game goes on.
- 17:34:59 a new shell (pid 8469) starts; the player presses Play and it launches again with
  `retro-8469-0.json`. `TaskRunnerHost.start` sends `FLAG_ACTIVITY_NEW_TASK`, the runner's task
  still stands, so Android brings the old game forward and drops the intent: no new runner, no
  `retro-8469-0.json`.
- 17:35:35 the Guide: `the game is held: the game has no control channel`. The shell has no port,
  so nothing reaches the runner.

## Cause

`src/app/retro_session.cpp:RetroSession` only knows the control file its own process names, and
`TaskRunnerHost.start` assumes no runner task stands, so a runner left by a dead shell is reached
by no one and swallows the next launch.

## Fix

A shell starting finds control files other shells left (`RetroSession::closeLeftovers`), has each
runner Save and close (`/suspend`: save RAM written, resume state taken) and quit (`/quit`: the
frame loop ends as a closed window ends it, and the runner's Activity with it), before any launch.
The next Play resumes from the resume state. Proof: on the phone, start a game, kill the shell's
process alone (`run-as <package> kill <shell pid>`), open openSU, Play again: the game resumes
where it was and the Guide holds it.
