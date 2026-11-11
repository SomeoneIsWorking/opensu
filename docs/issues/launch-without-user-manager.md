# No game starts without a reachable systemd user manager

Status: closed.

On Linux every launch ran through `systemd-run --user --scope`, the only Linux backend, picked at
build time. When systemd-run could not reach the user manager it exited at once, and
`ProcessLauncher::start` returned "could not start <program>" with no reason; the only cause on
record was systemd-run's stderr in openSU's log, `Failed to connect to user scope bus via local
transport: No such file or directory`. opensu-retro, emulators and Steam all failed alike. A
player on a distribution without systemd (Void, Alpine, Artix, Devuan, Gentoo with OpenRC), in a
container or chroot without the user bus, or in a login with no user manager (ssh or `su` without a
logind session) could start nothing. A systemd desktop, openSU's own session and the AppImage
there were never affected.

Reproduced in `scratch/zerotier-lab/moved/shell.log`: a hidden shell whose `XDG_RUNTIME_DIR`
pointed at a scratch directory and whose session bus was dropped (`tools/screenshots/shell.py`
`Profile` without `user_session`).

Fix: each process chooses its launchers' containment once at start-up
(`launch::chooseContainment`). It tries `systemd-run --user --scope` with the flags every launch
uses and logs the choice and why. When that fails, every launch runs under a keeper of its own
(`process_launcher_keeper.cpp`): a forked child of openSU that runs only syscalls, makes itself
the child subreaper of the game's tree, starts the game in its own process group and stays its
parent. A process that setsids or double-forks reparents to the keeper instead of escaping. Stop
SIGTERMs the group and every descendant, SIGKILLs what is left after 5 s, as the scope's
`TimeoutStopSec`, and reaps it all. The keeper also stops the tree when openSU's end of the pipe
between them closes, which happens only when openSU exits or crashes: both ends are `O_CLOEXEC`,
and every keeper closes every descriptor it inherited, so no other child holds it.
`PR_SET_PDEATHSIG` is not used because it fires when the forking thread exits, and openSU forks
from per-launch threads. The keeper exits with the game's code, or raises the game's signal on
itself with core dumps off, so `exitStatus` reads as it does from a scope. A start that fails
names the cause: "could not start opensu-retro: No such file or directory".

Proof, a hidden shell walking to a GBA game, playing it and closing it from the Guide:
- `scratch/zerotier-lab/keeper-run/` is the reproduction: no session bus, `XDG_RUNTIME_DIR` in
  scratch. The log reads `launches run under openSU's own keeper, as systemd user scopes cannot:
  systemd-run exited with 1: Failed to connect to user scope bus via local transport`, then
  `opensu-retro started under keeper <pid>`. The runner drew 483 frames, Close game toasted "link
  test closed", and no process named the ROM afterwards.
- `scratch/zerotier-lab/scope-run/` is the same walk with the session: `launches run in systemd
  user scopes`, and the game ran in its scope as before.
- `process_launcher_test` runs every check under both containments. For the keeper it also
  checks: a double fork into its own session is ended on stop; an orphan keeps the launch running
  after the started process exits; exit 7 and SIGSEGV come back as 7 and 139; a game that ignores
  SIGTERM is SIGKILLed after 5 s; a stand-in openSU SIGKILLed with two games running ends both
  trees.
