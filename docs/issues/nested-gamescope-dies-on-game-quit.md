# Nested Gamescope dies when a game quits

Status: fixed in the fork pin `79db8c0d` (the fix is `eea763bd`'s; the pin adds the hardening
below); unverified on the real KDE session.

Observed: quitting a game from its own menu (Absolute Drift via gogdl) ended openSU (exit 134),
Steam and the game; once also about 35 s into a fresh start with Steam starting. KWin logged
`error in client communication`, Gamescope `protocol error 3 on xdg_surface@52` and aborted in
`CWaylandInputThread::ThreadFunc`.

Cause: `WaylandBackend.cpp` `CWaylandPlane::Present` unmaps the toplevel with a NULL buffer when
steamcompmgr hides the connector (no focus window), then attaches the next frame's buffer without
waiting for the configure that an unmapped xdg_surface needs, which is xdg_surface error 3
(`unconfigured_buffer`).

Fix: upstream's series c50ddfa9 (skip the attach until a configure arrives), 6867f509 (only after
a real unmap) and e0602ccc (resend app_id and title after the unmap), cherry-picked onto the
fork's `opensu` branch.

Hardening on top (fork `opensu`, pin `79db8c0d`):
- b35aaa12, upstream PR 2393 (open): the input thread ends the session on a Wayland error instead
  of `abort()`; the `wl_display_get_fd` invariant keeps its abort.
- 883b7842: that shutdown exits with status 1 (`ShutdownGamescopeOnError`), so a lost host is
  not a clean quit to openSU.
- 79db8c0d: the host display fd's hangup no longer reaches `IWaitable::OnPollHangUp`, which
  aborted before the read could report the closed connection.
- openSU's `CompositorSession` starts a fresh Gamescope after an abnormal end (S013).

Audit of the other fatal paths in `WaylandBackend.cpp`: every input-thread failure is a dead host
connection (protocol error or closed socket) and still ends Gamescope; `assert`s are live
(`b_ndebug=false`) but check Gamescope's own invariants, not host input; the main thread ignores
its read errors and leaves them to the input thread.

Reproduce: `uv run --frozen python -m tools.nested_remap --gamescope <binary>` runs a private
headless KWin and openSU's nested command line with a child that closes and reopens a glxgears
window. At `41e84d4f` 5/5 runs aborted on the first reopen; at `eea763bd` 14/14 runs survived
(5 and 3 runs of 10 cycles, 6 of 40), the toplevel acking a new configure before each buffer.
`--scenario rapid|steam` (short-lived and overlapping windows) and `--focus-flap` (KWin minimizes
and reactivates Gamescope every 170 ms) found nothing new: 25/25 at `eea763bd`, 15/15 at
`79db8c0d`. `--host-exit-after 8` stops KWin mid-run: `eea763bd` 3/3 SIGABRT, `79db8c0d` 5/5 exit
1 with `Failed to read events on input thread: Connection reset by peer`. `41e84d4f` with the three
hardening commits still hits error 3 and now exits 1 with the protocol error logged (3/3).
