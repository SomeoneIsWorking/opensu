# Nested Gamescope pairs frames on a KWin desktop

Status: open. Not fixed; the cause below is a hypothesis, not proven.

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

## The player's direction

"Should also be able to dynamically adjust but gamescope doesn't support that right? So we might
need to mod gamescope OR openSU itself doesn't run under gamescope but uses gamescope as a layer
underneath hmm idk about this, maybe openSU should have its own compositor".

## Also

- The nested Gamescope's `-r` is fixed when it starts (`session::gamescopeArgs` from the monitor's
  mode), so docking or undocking keeps the old rate until openSU restarts. The proper fix is a root
  atom in our Gamescope fork that sets the nested refresh live, written when the output's mode
  changes.
- Neither applies in session mode, where Gamescope is the top level on DRM.
