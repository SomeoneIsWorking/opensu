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
- A QR code beside the Play online invite and reply codes, so a friend can take a code off a photo
  or another screen without reading it (`docs/issues/invite-code-legibility.md`).

## Libretro ports of standalone emulators

A libretro core of Eden (Switch) or Cemu (Wii U) would let the built-in runner own input, pacing,
shaders, presets and the Guide for those consoles, as it does for the cores it hosts. Neither exists;
each is a port of a large C++ emulator onto the libretro API (its renderer onto the runner's Vulkan
context, its threads onto `retro_run`), kept in a fork pinned by revision. Until then
`docs/plans/emulator-launch-config.md` sets the standalone builds up per launch.

## Native ports of console games

Fan-made native PC ports built from decompilations exist for some titles; openSU could offer one as
another way to run a game it lists, beside the emulator, on the player's own game data. What exists
for Super Mario Galaxy is in `docs/prior-art.md` ("Native ports of console games").
