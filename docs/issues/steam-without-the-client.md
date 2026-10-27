# A Legendary-style tool for Steam

Status: open, idea; nothing built.

The player: "I also want to make something like Legendary for Steam or add Steam to legendary".

What such a tool can and cannot replace:
- Can, without the Steam client: sign in (password plus Steam Guard, or the mobile app's QR),
  list owned apps, install, update, verify and uninstall them from Steam's CDN, and write the
  `appmanifest_<id>.acf` so the client accepts the files. SteamKit2 (LGPL-2.1, maintained) does
  the protocol; DepotDownloader (GPL-2.0) shows it working.
- Running games: a game that calls Steamworks (`SteamAPI_Init`) or is wrapped in SteamStub loads
  `steamclient` in process and asks it, through the SDK's public interfaces (`ISteamClient`,
  `ISteamUser`, `ISteamApps`, ...), whether the signed-in account owns the app, and for tickets.
- Goldberg-style emulators answer "owned" without asking anyone; that gets around the licence and
  is not an option.
- A real implementation is different: our own `steamclient` signed in to the player's account
  answers ownership from the account's licence list and requests app ownership, encrypted app and
  auth session tickets from Steam's servers, exactly as Valve's client does. An unowned game fails
  the same way it does under Steam.
- Its cost is the surface: each interface version games link against, plus achievements, stats,
  cloud saves and networking over the CM protocol (public protobufs, SteamDatabase/Protobufs);
  games using Steam's relay networking or the overlay are the hardest. It must be native (loaded in
  the game's process, through Proton's `lsteamclient` for Windows games), so C++, not SteamKit2.
- Risk: it is an unofficial client on the player's account. Valve tolerates SteamKit2 download
  tools; running games through one is less trodden.
- Until it exists, the Steam client stays as a background service at launch (as Isolated already
  runs it: no windows, openSU's own scope); the tool takes over the library, downloads and
  updates.

Not "add Steam to Legendary": Legendary's code is shaped around Epic's APIs (GPL-3, Python); a
Steam store in it would be a permanent fork with nothing shared but the CLI shape.

Decided direction if built: its own repository, a CLI with Legendary's command shape (`auth`,
`list`, `install`, `update`, `verify`, `uninstall`, `launch`, JSON output) so openSU drives it the
way it drives legendary and gogdl, written on SteamKit2, MIT.
