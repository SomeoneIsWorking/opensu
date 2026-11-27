# A games folder's `.desktop` lists as its file name, not the game it starts

Status: open.

Reported by the player (2026-11-27, Fedora): "my psx revamped games on fedora, openSU entries list
the .desktop files, imo we should take the contents of .desktop and not the .desktop itself, and
please reorganize my openSU library here too because they are listed as tomba_port but they should
be like Tomba Revamped Collection or something".

Expected: a `.desktop` in a games folder lists under the `Name` it gives, with its `Icon`, and
starts its `Exec`, so the PSX revamped ports read as their titles ("Tomba Revamped Collection"), not
`tomba_port`.
