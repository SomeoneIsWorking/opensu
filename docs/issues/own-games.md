# The player's own games: Add a game

Status: partial. Built and shown in the hidden product on the player's real setup (below). Project
state S045.

Requested: "how do I add games that aren't Steam, GoG, Epic or emulated". Then: "when I try to add
a game, try to guess what it is, you can test it on games in /mnt/Boy/Games", and "I will pick the
program, not the folder".

## What it does

- Library has an Add a game tile (after Giveaways, before All games). It offers "Choose a file"
  and, when there are any not imported yet, "Import N Steam shortcuts" and "Import N Lutris games".
  Each opens a pick list: a switch per game, all on at first, Select all, Deselect all, Undo of
  each change and Import selected, which adds only the games chosen; Back adds nothing.
- Choose a file opens the folder chooser in file mode: folders and the files `own::kindOf` accepts
  (an executable, a `.sh`, an `.AppImage`, a Windows `.exe`); the Path field takes a typed path.
  The picked file is what launches.
- Add a game also takes a shortcut (`.lnk`, `.url`, `.desktop`, `folder::shortcutEntry`): the entry
  starts what it names, with its arguments and folder, and its name is the first clue.
- Add a game folder keeps a folder in step with the library (`docs/guide/stores.md`).
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
  `artwork/own/<n>-<app>.jpg`); without one, SteamGridDB by title through `CoverFitter` when connected,
  else the placeholder; Choose artwork as for any game.
- Imports, read only: Steam's `userdata/*/config/shortcuts.vdf` (binary KeyValues, `vdf/binary`),
  and Lutris's `pga.db` opened `immutable`; a Lutris entry runs as
  `lutris lutris:rungameid/<id>`, through Lutris's own runner and settings. A Lutris game is the
  Steam app Lutris knows it as: its Steam service id, a Steam runner's `game.appid`, or
  `system.env.GAMEID` (`umu-<id>` or the bare id) in `games/<configpath>.yml`. An entry imported
  before that was read takes it from Lutris on each listing; a match the entry keeps wins.
- Match a different game, on a game's details and as X in Choose artwork: Steam's store
  search for the title, dropping its last words while it finds none, or a name typed after X;
  up to eight apps, each with its Steam portrait (else header) under the cache's
  `artwork/match/` and its release year from `appdetails`; the current match is ticked. A keeps
  the app as the game's `match` setting (`library/match_pick`, `game-settings.json`), which the
  Steam cover (`artwork/own/<n>-<app>.jpg`), the SteamGridDB lookup and the umu GAMEID follow;
  the entry's `steamAppId` stays what it was identified as when added. Store games and ROMs are
  matched the same way (`docs/project-state.md`, S045).
- A fitted SteamGridDB cover is kept with its lookup (`fitted/<id>.for`: Steam app and title);
  one fitted for another lookup, or kept without one, is dropped and fitted again. Cause of the
  Resident Evil 4 Remake showing the 2005 game: `lutris_games.cpp:lutrisGames` read only the name,
  so the game had no Steam app and `SteamGridDb::covers` took the first title match.
- Details of an own game: a File fact (or "Runs through: Lutris"), no Wine prefix row and no
  Uninstall, plus a Steam app fact, Match a different game, Rename, Change file and Remove from library, which asks first and says the file
  stays. After a removal the focus goes to the tile now in the removed game's slot; when the last
  one goes, Your games goes with it and Library is focused on the tile now in its slot.

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
