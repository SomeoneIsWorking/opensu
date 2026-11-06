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
