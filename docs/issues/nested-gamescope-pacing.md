# Nested Gamescope pairs frames on a KWin desktop

Status: open.

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

## Also

- The nested Gamescope's `-r` is fixed when it starts (`session::gamescopeArgs` from the monitor's
  mode), so docking or undocking keeps the old rate until openSU restarts. The proper fix is a root
  atom in our Gamescope fork that sets the nested refresh live, written when the output's mode
  changes.
- Neither applies in session mode, where Gamescope is the top level on DRM.
