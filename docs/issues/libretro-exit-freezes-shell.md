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

Not fixed here; each can block the main thread, but neither matches this journal.

- `src/app/shell_app.cpp:ShellApp::closeRunningGame` calls `Handoff::forceClose`, which calls
  `src/launch/instance.cpp:Instance::kill` on the main thread. `Instance::endScope` then calls
  `Instance::reapChild`, which polls `waitpid` for up to 10 s (`reapWait`), then sends SIGKILL and
  calls a blocking `waitpid` with no timeout. A child stuck in uninterruptible sleep blocks the
  shell forever. Here the close returned within 62 ms.
- `src/app/focus_follow.cpp:FocusFollow::service` assigns a new `std::jthread` to `closer_` when
  Big Picture takes focus. If the previous `closeBigPicture` thread is still running, the move
  assignment joins it on the main thread, so a second Big Picture focus during a slow Steam
  blocks the shell until the first close returns. No Big Picture focus was logged here.
