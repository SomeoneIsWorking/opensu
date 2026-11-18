# One game, many versions

Requested 2026-11-17, relayed from the user by the macOS session: "make LAN games first-class next
to local ones" (one tile per game across this openSU and paired ones, remote marks on tiles, the
paired openSUs' Home on this Home), then "group versions of one game across platforms and
sources". The example on the user's PC: X-Men Legends II shows as four tiles, a PS2 ROM
(`X-Men Legends II - Rise of Apocalypse (USA).chd`), an Xbox ROM (`… (USA, Europe).iso`), a native
port (`xmen2-port.desktop`, "X-Men Legends II") and a Lutris Wine install ("X-Men Legends II: Rise of
Apocalypse"). They are to be one tile.

## What it does

- **A game is a group of versions.** Every version of one game, from any source, is one tile on
  Home, in All games and in a launcher's own shelf: ROMs of any console, a game added from a file or
  imported from Lutris or a shortcut, store copies, and the copies on paired openSUs. A console's
  shelf and a launcher's shelf still list their own versions, each standing for its group.
- **Matching.** Two versions are one game when their titles compare equal: case, punctuation,
  trademark signs do not count, "&" reads "and", and a ROM's region, disc and revision tags never
  reach its title (`roms::cleanTitle` at the scan). A title and the same title with a subtitle
  ("X-Men Legends II" and "X-Men Legends II - Rise of Apocalypse" or "…: Rise of Apocalypse") are
  one game when only one subtitled game shares that base title. A store holds one version of a game,
  so two same-titled games of one store stay two. A store or LAN game also matches by its store and
  store id; a ROM on a paired openSU by the key shared saves use (Switch by title id, others by ROM
  name).
- **The player's fixes.** "Group with…" on a game's page picks another game to join; "Separate" on
  a version takes it out into a game of its own. Both are kept by version id (`versions` in
  `settings.json`), so a rescan does not undo them.
- **The default version.** Play on the tile starts the group's default: the one the player made
  default ("Make default" on its row), else installed before not, local before remote, most
  recently played, then Steam, GOG, Epic, the player's own, ROMs. The tile's cover, title and
  settings follow the default.
- **Extras.** "Move to Extras" on a version keeps it installed and playable from the page's Extras
  section, but it is never the tile, the default, or counted on Home and in Library. "Move back"
  undoes it. A group whose versions are all in Extras but one is just that one; a group whose
  versions are all in Extras shows its first. Kept per version id.
- **The details page** lists the versions, the default first, each with where it comes from:
  "PlayStation 2 · LRPS2", "Windows · Wine", "Linux · AppImage", "Linux", "Steam", "On Desk"; a
  Lutris game by what it runs as, since openSU runs it from Lutris's settings itself. The default's Play is
  the page's; each other version has Play, Make default, Move to Extras and Separate. A version on a
  paired openSU reads "Play on Desk", or says why it cannot be played there (that openSU cannot
  stream: it is not Linux in Gamescope).
- **What a paired openSU tells about its games.** Each game's platform and source (Steam, GOG,
  Epic, Lutris or Wine, a native port, a ROM and its emulator), shown on its tile and page, and the
  ways it can launch it. Each launch option is a version of its own here: "Play on Desk: Steam",
  "Play on Desk: DuckStation", "Play on Desk: libretro (SwanStation)". `/lan/play` names the option
  chosen, and that openSU refuses one it does not offer.
- **Remote marks.** A tile whose game is only on paired openSUs carries a remote badge over its
  store's icon (Steam, GOG, Epic) or its console's art. A tile with a version here and one on
  another openSU says "Also on Desk" quietly.
- **Paired openSUs' Home.** Each paired openSU's Home follows this Home as a section of its own,
  "From Desk", in that openSU's order, Online or Offline as its launcher tile says now. A game that
  is also here shows as its group's tile, not twice.

## Owners

- `library/titles`: the one owner of "the same game". Its `Title` becomes the group of versions
  across every source; matching, the player's fixes, the default and Extras are applied there and
  nowhere else. `TitleCopies` is the details page's and launch's view of it.
- `library/version_choices`: the player's groupings, separations, defaults and Extras by version id,
  kept in `settings.json` (`versions`). `app/game_versions` applies a page's change to them.
- `lan/wire` `ListedGame`: gains `same` (the same-game key), its platform, source and launch
  options; the listing gains `hosting` (empty, or why that openSU cannot stream) and its Home order.
  `/lan/play` gains the launch option.
- `ui/game_details`: the versions list and its rows. `ui/home_grid` and the tile painter: the
  remote badge and "Also on". `ui/home_layout`: sections on Home.

## Order

1. Versions here: matching, Group with, Separate, default, Extras, the details page. Shown on a
   fixture with the X-Men Legends II shapes: a PS2 and an Xbox ROM, a Lutris game and a `.desktop`
   port, one tile with four versions.
2. LAN copies as versions: the same-game key, hosting, platform, source and launch options on the
   wire, "Play on <name>: <option>". Shown with two hidden openSUs paired over Avahi.
3. Remote marks on tiles, with the platform and source.
4. Paired openSUs' Home on this Home. What a paired openSU sees gains its Home order, and each
   game's platform, source and launch options
   ([lan.md](../guide/lan.md#what-is-shared-and-with-whom)).

## State

Step 1 built: grouping, Group with…, Separate, the default, Extras and the Versions card, shown
on the X-Men Legends II fixture (two ROMs, a `.desktop` port, an AppImage and a Lutris install as
one tile). Each tile carries one runner badge for its default version (`ui::runnerMark`): shown
with xemu's icon from its desktop entry, Dolphin's from its theme icon and DuckStation's
initials. An AppImage emulator's icon inside its image is not read yet.

Step 2 built: the listing carries each game's platform, source and emulators and the host's
`hosting`; `lan::remoteVersions` makes one version per emulator, `/lan/play` names it and the host
refuses one it does not list, then launches with it (`library::runOn`).

Step 3 built: another openSU's default version carries its runner there, ringed
(`ui::runnerMark`), and the focused title says "On Desk" or "Also on Desk" (`ui::whereElse`,
from `TitleCopies::hostsOf`). Shown in `tools.nat_lab lan-versions`. Next: step 4.
