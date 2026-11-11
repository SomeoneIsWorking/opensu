# Closing a libretro game freezes the shell

Status: open, investigating. Cause unknown.

Observed: inside openSU's nested Gamescope, the user quit Kirby & the Amazing Mirror (mGBA core on
`opensu-retro`) with the quick menu's Close game; the shell froze and never drew again.

## Evidence

Journal of session `opensu-1476366` (shell pid 1476401, runner pid 1481804), 2026-10-16:

```
20:06:23.928 [launch] closing Kirby & the Amazing Mirror from a menu
20:06:23.985 [session] focus on 0x0: openSU
20:06:23.990 [launch] Kirby & the Amazing Mirror finished
20:06:23.990 [launch] Kirby & the Amazing Mirror is over: Kirby & the Amazing Mirror closed
20:06:24.048 [gamepad:warn] open /dev/input/event22: No such file or directory
20:06:24.048 [gamepad:warn] open /dev/input/event23: No such file or directory
20:06:24.052 [plugin.eden-playtime] play time for 11 of 27 Switch games
```

- Every earlier quit in that session handed focus back to the shell's own window
  (`focus on 0x60002f: openSU` at 20:04:18, 20:04:48 and 20:05:26). This one reports window `0x0`.
- The close itself returned: the kill, the focus line, the pads release (the runner's virtual pads
  22 and 23 are already gone) and the play-time plugin all ran within 125 ms.
- After 20:06:24.052 the main loop logs nothing. The Steam watcher thread still logs at 20:07:02 to
  20:07:04, so the process was alive and only the main thread had stopped.
- The user's close requests to `0x60002f` (Gamescope `--close-focused-window`) got no answer;
  the journal has no line for them. At 20:07:22 Gamescope reports `Primary child shut down!` and the
  session ends.

## Hidden runs inside Gamescope

A plain `opensu --hidden` has no Gamescope, so the path above never runs there. Inside a headless
Gamescope, a hidden shell used to skip it as well: `ShellApp` attached `GameScreen`'s overlay and
`FocusFollow` only to a shown window. It now attaches both whenever it runs in Gamescope, and only
its window stays unmapped.

Attempt on 2026-11-10, on that change (`scratch/exit-freeze/tools/freeze.py`, the run in
`scratch/exit-freeze/run1/` of that session):

- Setup: `gamescope --backend headless -W 1280 -H 720 -r 60 -- opensu --hidden`, with
  `WAYLAND_DISPLAY` empty and the desktop's `DISPLAY`, a scratch home, Kirby & the Amazing Mirror
  on the shipped mGBA core.
- Four rounds: Play, 370 to 720 frames of the game, then Guide, Close game, A, A.
- After each close, `/frame.png` (drawn by the main loop only) was asked three times, one second
  apart. Every answer came within 79 to 105 ms and showed the shell's page with "Kirby & the Amazing
  Mirror closed".
- The overlay path ran: the log has `overlay window 0x…, depth 32`, then each round
  `focus on 0x…: the game` and, after the close, `focus on 0x0: openSU`.
- So window `0x0` after a close is the normal case here, not the sign of the freeze.

No freeze. That run already took the player's present path: inside a Gamescope a hidden shell has
not passed `--hidden` to the runner since remote play landed (b8e05dcd), and its log has the Gamescope
WSI swapchain on the runner's window (`flip: true`) and "the first frame is on screen". The note that
it drew offscreen was wrong. What that runner did take from the player's session was the default
sound device and every pad on the machine; it now starts with `--silent` (`roms::RunnerMode`,
`docs/design/libretro.md` Command line).

Attempt on 2026-11-11 with `--silent` (`scratch/runner-surface/` and `scratch/exit-freeze/run2/` of
that session):

- Gamescope's own screenshot (`gamescopectl screenshot`) during play shows Kirby's title screen
  drawn by the runner, before the change and after (`runner-surface/before|after/frames/game-gamescope.png`).
- The runner's arguments carry `--silent`; PipeWire lists no `opensu-retro` stream for it, where
  the same runner without the flag in a headless Gamescope has two `Stream/Output/Audio` nodes
  (`runner-surface/player-mode/`).
- `freeze.py`, four rounds: each `/frame.png` after a close answered in 56 to 84 ms. No freeze.

## Next time it freezes

Leave the frozen shell running and, from a desktop terminal:

```
ps -C opensu,opensu-retro,gamescope -o pid,ppid,stat,wchan:32,args
# the shell is the opensu whose parent is gamescope
pid=<that pid>
gdb -p $pid -batch -ex 'set pagination off' -ex 'info threads' -ex 'thread apply all bt full' \
  > ~/opensu-freeze-gdb.txt 2>&1
cat /proc/$pid/task/*/wchan; echo
journalctl --user -b -o short-precise --since '-10 min' > ~/opensu-freeze-journal.txt
```

Attach to the shell's main thread first: thread 1 in `info threads` is where the loop stopped.
`kernel.yama.ptrace_scope` is 0 on this machine, so no sudo is needed.

## Side defects found on the way

Both fixed; neither matched this journal, so the freeze itself stays open.

- Close game waited on the main thread. `Handoff::forceClose` now only raises the request and wakes
  the launch's thread, which stops the scope (`Instance::stop`: SIGTERM, SIGKILL after 5 s), waits for the started process
  (`reapChild`: up to 10 s, then SIGKILL and a blocking `waitpid`) and ends the launch through the
  usual game-end path. A hidden run with a stand-in emulator that ignores SIGTERM and a `systemctl`
  whose `kill` does nothing, so the process outlives its scope for the full 10 s
  (`scratch/dollar/runs/close-fault-before/` and `close-fault-after/`): before, `/frame.png`, which
  only the main loop answers, timed out twice at 5 s and the loop drew nothing for 12 s; after, it
  answered 174 times in those 15 s, slowest 84 ms. `/state` stays under 25 ms in both, because it
  is a snapshot the loop publishes and cannot show a stalled loop.
- `FocusFollow::service` no longer assigns over a running `std::jthread`. One closer thread,
  started on the first Big Picture focus, calls `closeBigPicture` one at a time; a focus while a
  close runs marks one more close after it, so the main thread never waits for Steam.
  `focus_owner_test` holds the first close and checks the second focus is answered while it
  runs. There is no seam for this in a hidden run: `FocusFollow` gets a focus source only inside
  Gamescope.

## Not the SIGTERM lifetime crash

`shell-sigterm-crash-with-game.md` is a launch thread outliving `ShellApp`; it ends in SIGSEGV,
only while the shell is being torn down. In this journal `ShellApp` is alive: the launch's end
notice was posted (`is over … closed`), the Steam watcher thread still logs 40 s later, and only
the main loop stopped. A use after free would have crashed the process, not stalled one thread,
so that fix does not explain this freeze.
