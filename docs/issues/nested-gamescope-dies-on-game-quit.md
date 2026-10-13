# Nested Gamescope dies when a game quits

Status: fixed in the fork pin `eea763bd`; unverified on the real KDE session.

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

Reproduce: `uv run --frozen python -m tools.nested_remap --gamescope <binary>` runs a private
headless KWin and openSU's nested command line with a child that closes and reopens a glxgears
window. At `41e84d4f` 5/5 runs aborted on the first reopen; at `eea763bd` 14/14 runs survived
(5 and 3 runs of 10 cycles, 6 of 40), the toplevel acking a new configure before each buffer.
