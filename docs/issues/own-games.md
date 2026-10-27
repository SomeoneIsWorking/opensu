# The player's own games: Add a game

Status: partial. Built and shown in the hidden product on the player's real setup (below). Project
state S045.

Requested: "how do I add games that aren't Steam, GoG, Epic or emulated". Then: "when I try to add
a game, try to guess what it is, you can test it on games in /mnt/Boy/Games", and "I will pick the
program, not the folder".

## What it does

- Library has an Add a game tile (after Giveaways, before All games). It offers "Choose a file"
  and, when there are any not imported yet, "Import N Steam shortcuts" and "Import N Lutris games".
- Choose a file opens the folder chooser in file mode: folders and the files `own::kindOf` accepts
  (an executable, a `.sh`, an `.AppImage`, a Windows `.exe`); the Path field takes a typed path.
  The picked file is what launches.
- The file is identified on a worker (`library/game_identify`): clues on disk first
  (`steam_appid.txt`, a GOG `goggame-*.info`, a `.desktop` entry that starts the file, the folders
  above it up to a library folder such as `Games` or `AppImages`, skipping `Game`, `Binaries`,
  `Win64` and the like, the .exe's version resource unless it is an engine placeholder such as
  `BootstrapPackagedGame`, the file's name without build words such as `_Steam`), then each clue
  in Steam's store search (no key) until one equals a store title (`titles::titleKey`), or the
  app of `steam_appid.txt`. The confirm card says the platform, the Steam app and where the title
  came from, with the title in a field; Done adds, Close does not.
- An entry is kept in `<data dir>/own-games.json`, never in the game's folder; ids are never given
  twice, so a removed game's cover, last played and prefix never reach a new one.
- A Windows `.exe` runs under `wine::prepare`'s runner (pin, pick, Automatic) as `umu-run <exe>` or
  `<wine> <exe>`, always in `<data dir>/wine-prefixes/own-<n>`, started in the file's folder. A
  non-executable script runs through its `#!` interpreter; a non-executable AppImage or program says
  to `chmod +x` it. A missing file points at Change file.
- Art: a matched Steam app's portrait from Steam's CDN (`ArtworkFetcher`, cached under
  `artwork/own/<n>.jpg`); without one, SteamGridDB by title through `CoverFitter` when connected,
  else the placeholder; Choose artwork as for any game.
- Imports, read only: Steam's `userdata/*/config/shortcuts.vdf` (binary KeyValues, `vdf/binary`),
  and Lutris's `pga.db` opened `immutable`; a Lutris entry runs as
  `lutris lutris:rungameid/<id>`, through Lutris's own runner and settings.
- Details of an own game: a File fact (or "Runs through: Lutris"), no Wine prefix row and no
  Uninstall, plus Rename, Change file and Remove from library, which asks first and says the file
  stays.

## Checked on the player's setup

Hidden openSU from the worktree build in a headless Gamescope, real XDG dirs, children with
`WAYLAND_DISPLAY` empty and `DISABLE_LSFG=1`, driven through the control channel
(`scratch/own-games/run.py`). Frames in `scratch/own-games/frames/`.

Each game's real executable in `/mnt/Boy/Games`, picked by typed path (`r6-confirm-*.png`, the
grid with covers `r6-your-games.png`):

| File | Guessed title | Steam app | Platform | Cover | Right |
| --- | --- | --- | --- | --- | --- |
| `Clair Obscur - Expedition 33/Expedition33_Steam.exe` | Clair Obscur: Expedition 33 | 1903340 | Windows | Steam | yes |
| `ELDEN RING/Game/eldenring.exe` | ELDEN RING | 1245620 | Windows | Steam | yes |
| `Lies of P/LOP.exe` | Lies of P | 1627720 | Windows | Steam | yes |
| `Little Kitty, Big City/Little Kitty, Big City.exe` | Little Kitty, Big City | 1177980 | Windows | Steam | yes |
| `Need For Speed Most Wanted Remaster Edition/speed.exe` | Need For Speed Most Wanted Remaster Edition | none | Windows | placeholder | title yes; the 2005 game is not on Steam |
| `Resident Evil 4/re4.exe` | Resident Evil 4 | 2050650 | Windows | Steam | yes |
| `Sackboy - A Big Adventure/Sackboy.exe` | Sackboy: A Big Adventure | 1599660 | Windows | Steam | yes |
| `X-Men Legends II/XMen2.exe` | X-Men Legends II | none | Windows | placeholder | title yes; not on Steam |

- Little Kitty, Big City launched from its details under GE-Proton11-11 in
  `wine-prefixes/own-6` and showed its window (`r8-play-Little Kitty, Big City-gamescope.png`, the
  game's loading screen from Gamescope's PipeWire stream); closed from the Guide menu.
- `~/AppImages/nebula-reset.appimage`, added by typed path, showed its title screen
  (`r4-play-Nebula Reset-gamescope.png`).
- `~/Games/ApeOut/ApeOut.exe` (added by browsing, `r1-*.png`) ran under GE-Proton but exits: the
  copy has no `UnityPlayer.dll` (Proton log), so it is a broken install, not a launch fault.
- Imported 12 Steam shortcuts and 9 Lutris games (`r9-*`); removed two entries through Remove from
  library (`r9-remove-ApeOut.png`).
- Nothing was written in `/mnt/Boy/Games`. Afterwards `own-games.json`, `wine-prefixes/` and
  `artwork/own/` were deleted and the test runs' `lastPlayed` entries taken out, so the library is
  as it was.

## Not reached

- SteamGridDB art for own games: SteamGridDB is not connected on this machine.
- Launching a Lutris entry (it would update Lutris's own database) or a Steam-shortcut entry.
- Imported entries are not identified; they keep the title Steam or Lutris gives and have no cover
  until Choose artwork.
- Epic's `.egstore` metadata is not read (it holds no title).
