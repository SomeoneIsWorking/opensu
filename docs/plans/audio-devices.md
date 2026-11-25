# Audio devices, per-game audio, and one volume display

Requested 2026-11-25 from the Ally: "showing audio device options and being able to select audio
output and input and configuring them individually and maybe also per-game audio", "if in desktop
mode (not in session mode) and the current DE has its own volume display, we don't show ours", and
"I don't understand why you show some options like Audio but then say it's in guide ... just
duplicate it there".

## What it does

1. **Every Guide-only setting also in Settings or Devices.** Rows that say "In the Guide menu"
   (Devices › Display's brightness and interface size, the volume) become the same controls the
   Guide has, each built by one owner that both use (as `interface_size_setting` does).
2. **Devices › Audio** lists outputs and inputs (PipeWire sinks and sources through
   `audio::VolumeBackend`): pick the default of each, and each one's own volume and mute.
3. **Per-game audio**: a game setting for its output device (and volume), applied to the game's
   streams when it starts.
4. **One volume display**: outside openSU's own session, where the desktop shows its own volume
   display (KDE Plasma's plasma-pa), openSU's overlay stays hidden; in session mode it is the only
   one.

## Found on the Ally (Bazzite), not openSU's

Bazzite's ROG Ally DSP is a PipeWire filter chain: a virtual sink "ROG Ally" feeding "ROG Ally
corrected" into the hardware sink (Ryzen HD Audio Controller Analog Stereo). What is heard is the
filter sink's volume times the hardware sink's, so KDE shows two devices whose levels multiply.
The fix is one of them held at 100% and the other made the default that every volume control
moves.
