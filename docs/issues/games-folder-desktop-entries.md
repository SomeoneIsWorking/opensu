# A games folder's `.desktop` lists as its file name, not the game it starts

Status: open.

Reported by the player (2026-11-27, Fedora): "my psx revamped games on fedora, openSU entries list
the .desktop files, imo we should take the contents of .desktop and not the .desktop itself, and
please reorganize my openSU library here too because they are listed as tomba_port but they should
be like Tomba Revamped Collection or something".

Expected: a `.desktop` in a games folder lists under the `Name` it gives, with its `Icon`, and
starts its `Exec`, so the PSX revamped ports read as their titles ("Tomba Revamped Collection"), not
`tomba_port`.

Seen (2026-11-27):

- Fedora: `~/Games/Projects/dev.someoneisworking.tomba.desktop` lists as "Tomba! Classic Collection",
  but its details page's File is the `.desktop` itself, not the program it runs.
- Ally: `~/Games/tomba2_port/` lists as `tomba2_port`, a subfolder game running its `play.sh`; the
  `.desktop` in `~/.local/share/applications` that runs that same `play.sh` and names it "Tomba!
  Classic Collection" (with its icon and box art) is never matched to it. The same for every
  `*_port` folder there.
