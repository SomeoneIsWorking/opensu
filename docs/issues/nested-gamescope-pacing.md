# Nested Gamescope pairs frames on a KWin desktop

Status: open. Not reproduced on a64e3707 on the Ally panel (2026-11-27); the VRR cause below is ruled out there.

Reported by the player (2026-11-24, Ally, desktop mode): "UI also seems laggy like not really
smooth, Ally is 120hz supposedly but these menu selection box movements look 30fps"; then
"sometimes framerate goes back to normal but sometimes it's visibly low but try to test over the
120hz Ally's own screen"; and (2026-11-26) "I said openSU UI feels like 30fps and you traced it to
kwin and gamescope"; and, on being told running nested costs smoothness, "which should be fixed"
(2026-11-26, Fedora).

On a KDE desktop (the Ally in Bazzite's desktop mode), openSU's nested Gamescope hands KWin some
frames in pairs: one present waits two refreshes and the next returns at once, so motion steps at
half rate for a moment. The shell's own presents are even; the pairs come from the nested
Gamescope.

## Evidence (2026-11-26, Ally, MangoHud frame-time logs)

- `vkcube` straight on KWin, TV at 60 Hz: 467 of 479 frames 16-20 ms, no pairs. On the 120 Hz
  panel: 934 of 959 frames 8-12 ms.
- `vkcube` inside the bundled Gamescope (`-r 60`, `DISABLE_GAMESCOPE_WSI=1`) on the same TV: 83 of
  479 frames are a 33 ms + 0.4 ms pair.
- The shell inside the same Gamescope: 114 of 720 frames paired at 60 Hz on the TV; on the panel at
  120 Hz, about 150 of 1439.

## Suspected cause (unproven)

KWin's automatic adaptive sync turns VRR on for fullscreen windows, and openSU starts the nested
Gamescope with `-f`. With VRR on, KWin reports `refresh=0` in `wp_presentation` feedback; the fork's
`WaylandBackend.cpp:Wayland_PresentationFeedback_Presented` passes each feedback to `MarkVBlank`,
which re-anchors the fixed vblank timer (`vblankmanager.cpp:CalcNextWakeupTime`) and produces the
pairs. A headless `kwin_wayland --virtual` run showed no pairs, which fits (no VRR there).

Next falsifier, on the Ally in desktop mode: the same MangoHud log with KDE's Adaptive Sync set to
Never, then with a windowed (not `-f`) nested Gamescope. Pairs gone in both points at VRR; pairs
still there rules it out.

## Against the VRR suspicion (2026-11-27, Ally)

`kscreen-doctor -o` reports the TV (DP-2) as `Vrr: incapable`, so KWin cannot have turned VRR on
there, yet the TV runs above showed pairs at 60 Hz. VRR may still matter on the panel (eDP-1,
`Vrr: Automatic`), but it cannot explain the TV pairs.

MangoHud loads into the shell and into `vkcube` under the nested Gamescope but wrote no frame log
in any run, so the timings below come from libwayland's protocol log (`WAYLAND_DEBUG=client`) on
both connections: the shell to the nested Gamescope, and the nested Gamescope to KWin.

## Not reproduced on a64e3707 (2026-11-27, Ally panel, 120 Hz, VRR Automatic)

- Nested Gamescope to KWin, `vkcube` inside: 1400 of 1403 `wp_presentation` intervals 8 ms, every
  feedback with `refresh=0`. KWin's VRR feedback does not pair the nested Gamescope's frames.
- `vkcube --wsi wayland` to the nested Gamescope: 1406 of 1412 commits 8 ms apart.
- The shell, selection moving for 70 s: Gamescope's frame callbacks reach it every 8 ms, it commits
  0-3 ms after each, and the nested Gamescope presents to KWin every 8 ms (1990 of 1999). After the
  start, 4 of 8447 frames miss a callback.
- The shell stalls 170-410 ms six times in the first 1.6 s after its window appears.

The layer SDL's GPU debug mode asked for (dropped in 8eda043e) is not installed on the Ally, so
that change does not explain the difference from the 2026-11-26 runs. The TV path (DP-2, 60 Hz)
is not checked again: the TV was off. If the player still sees it, the next run needs the screen
it happens on and whether openSU started docked or undocked (the fixed `-r` below).

Method: start openSU with `WAYLAND_DEBUG=client` in the unit's environment; the nested log
(`~/.local/state/opensu/gamescope-nested.log`) then holds both connections, Gamescope's with
`[ms]` stamps and the shell's with `[HH:MM:SS.us]` stamps (the AppImage's libwayland against the
host's).

## The player's direction

"Should also be able to dynamically adjust but gamescope doesn't support that right? So we might
need to mod gamescope OR openSU itself doesn't run under gamescope but uses gamescope as a layer
underneath hmm idk about this, maybe openSU should have its own compositor".

## Also

- `-r` (`session::gamescopeArgs`, from the monitor's mode) is only the starting rate: the fork's
  Wayland backend follows the host after that (`WaylandBackend.cpp`: the presentation feedback's
  refresh on a fixed-rate output, the entered outputs' modes under VRR; "Changed refresh to" in
  the nested log). Unchecked on a dock change: whether the shell follows, since
  `ShellApp::presentInterval` is read in `showGamePicture` and the nested `wl_output` gives its
  refresh when a client binds it.
- Neither applies in session mode, where Gamescope is the top level on DRM.
