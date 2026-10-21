# Steam imports Gamescope screenshots

Status: open.

With openSU's Steam running (Managed or Isolated), each `gamescope_control take_screenshot` is
picked up by Steam: it moves the file into `userdata/<id>/760/remote/7/screenshots/`, adds it to
`screenshots.vdf` and deletes the original. The Isolated proof runs (2026-10-21) left twelve such
screenshots in the player's Steam library, and a run before logon created `userdata/0/760`.

The player's own screenshot of openSU would end up in Steam's library too. Find where Steam learns
of the file (Gamescope's screenshot atom or its default path) and keep openSU's tool shots out of it.
