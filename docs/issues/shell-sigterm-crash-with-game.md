# Shell crashes on SIGTERM while a game runs

Status: open. Not traced.

Observed: SIGTERM to a hidden `opensu` while a libretro game runs ends it with SIGSEGV instead of a
clean exit. Expected: the shell closes the game and exits 0.

Reproduction: hidden shell in a scratch HOME, launch a SNES game on `opensu-retro`, send SIGTERM to
the shell's PID. Seen in `scratch/framegen-settings/runs/outside-*` and on the build before that
work (`build/agent-seams`, 2 of 3 runs in `runs/outside-base-*`), so frame generation is not the
cause.

The core dump's crashing thread is the launch thread:
`std::string::empty` ← `launch::Handoff::stillRunning` ← `launch::Handoff::start` ← the
`GameLaunch` thread lambda, while the main thread is in `app::ShellApp`'s destructor. The likely
cause is a member that thread reads being destroyed before the thread is joined; that is not yet
confirmed against the member order of `ShellApp` and `GameLaunch`.
