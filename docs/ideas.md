# Ideas

Maybes the player has raised but not decided. Nothing here is planned; a decided idea moves to
`docs/plans/`.

- Run Steam games through fumarole instead of Isolated Steam, so a game is openSU's own child: no
  kept virtual pads for the sandbox, the sound sink's environment reaches it, and no client to
  hide. Windows games under Wine only until fumarole has a native `steamclient.so`; Steam Input
  and anti-cheat games would stay on the client.
- Local Steam games on Android, which needs fumarole: the Steam client doesn't run there.
- A touch layout for shooters that is all keyboard and mouse, with no pad, so a game's prompts
  never flip between the two: a swipe moves the mouse, since a right stick turns worse (Risk of
  Rain 2), and each on-screen control is a key or mouse button. Needs a virtual keyboard and mouse
  on the host, and a key for each control per game.
- Frame generation on the remote guest: the host streams at 30 fps (half the bit rate and encode
  load) or at a 60 fps game's own rate, and the guest generates the frames between up to its
  screen's rate (60, 90 or 120 Hz on phones and tablets). Needs the generator out of the Gamescope
  fork into one library both link, the guest's decoded frames in Vulkan images it can run on
  (MediaCodec's `AHardwareBuffer` imported into Vulkan, the interop SDL GPU does not offer), and
  the LSFG 3 modules of the player's own `Lossless.dll` sent from the host to their own guest.
  Interpolation holds back one real frame: about 33 ms more at 30 fps, 17 ms at 60, plus the
  generation, which a phone GPU has to fit in its frame.
