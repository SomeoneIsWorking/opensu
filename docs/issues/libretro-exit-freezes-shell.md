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

## Why a hidden run cannot reproduce it

`opensu --hidden` has no Gamescope, so the path above never runs there: no overlay
(`ShellApp` attaches `GameScreen`'s overlay and `FocusFollow`'s source only inside Gamescope), no
focus following, and the runner draws offscreen with no Gamescope WSI surface. A hidden run of the
same close answers `/state` right after the game ends (`scratch/retro-freeze/`,
`scratch/retro-hidden/`). A reproduction needs the nested Gamescope session.

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
