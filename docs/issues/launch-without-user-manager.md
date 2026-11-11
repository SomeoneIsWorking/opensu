# No game starts without a reachable systemd user manager

Status: open.

On Linux `src/launch/process_launcher_systemd_scope.cpp:ProcessLauncher::start` is the only
launcher backend (picked at build time in `src/launch/CMakeLists.txt`): every launch runs through
`systemd-run --user --scope`. When systemd-run cannot reach the user manager it exits at once, and
start() returns "could not start <program>" with no reason; the toast shows just that, and the only
cause on record is systemd-run's own stderr in openSU's log, `Failed to connect to user scope bus
via local transport: No such file or directory`. There is no fallback, so opensu-retro, emulators
and every other program fail alike.

A player on a systemd desktop or openSU's own session has the manager, including from the
AppImage, which leaves `XDG_RUNTIME_DIR` and `DBUS_SESSION_BUS_ADDRESS` as the session set them.
A player hits this on a distribution without systemd (Void, Alpine, Artix, Devuan, Gentoo with
OpenRC), in a container or chroot without the user bus, or in a login with no user manager (ssh or
`su` without a logind session).

Reproduced in `scratch/zerotier-lab/moved/shell.log`: a hidden shell whose `XDG_RUNTIME_DIR` pointed
at a scratch directory and whose session bus was dropped (`tools/screenshots/shell.py` `Profile`
without `user_session`). The same walk with `user_session=True`
(`scratch/zerotier-lab/moved-fixed/`) logs `opensu-retro started in opensu-<pid>-game-0.scope`
and the runner draws 482 frames.

Proper fix, not yet chosen: either the Linux launcher starts the program in a process group (the
macOS backend's behaviour, losing the cgroup ownership of setsid and double-fork children) when no
user manager answers, decided once at start-up; or openSU refuses at start-up with the reason and
what to install. Either way start() should name the cause instead of the bare "could not start".
