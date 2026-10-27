# A Legendary-style tool for Steam

Status: open, idea; nothing built.

The player: "I also want to make something like Legendary for Steam or add Steam to legendary".

What such a tool can and cannot replace:
- Can, without the Steam client: sign in (password plus Steam Guard, or the mobile app's QR),
  list owned apps, install, update, verify and uninstall them from Steam's CDN, and write the
  `appmanifest_<id>.acf` so the client accepts the files. SteamKit2 (LGPL-2.1, maintained) does
  the protocol; DepotDownloader (GPL-2.0) shows it working.
- Cannot: run most games. A game that calls Steamworks (`SteamAPI_Init`) or is wrapped in
  SteamStub needs a running, signed-in Steam client holding the licence. Only DRM-free games run
  without it. Emulating Steamworks (Goldberg and the like) gets around the licence check and is
  not an option.
- So the client stays as a background service at launch (as Isolated already runs it: no windows,
  openSU's own scope); the tool takes over the library, downloads and updates.

Not "add Steam to Legendary": Legendary's code is shaped around Epic's APIs (GPL-3, Python); a
Steam store in it would be a permanent fork with nothing shared but the CLI shape.

Decided direction if built: its own repository, a CLI with Legendary's command shape (`auth`,
`list`, `install`, `update`, `verify`, `uninstall`, `launch`, JSON output) so openSU drives it the
way it drives legendary and gogdl, written on SteamKit2, MIT.
