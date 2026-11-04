# Cloud saves

Requested 2026-11-03: "Cloud saves could also be nice".

## What it does

Saves follow the player between their machines (Linux, Windows, macOS, Android) without thinking
about it: pulled before a game starts, pushed after it ends, with the same rules fumarole uses for
Steam Cloud (change numbers decide, a save changed on both sides is never overwritten and the
player picks which to keep, every replaced copy is backed up first).

## Where saves go, per source

- Store games use their store's own cloud: Steam Cloud through fumarole (built), Epic through
  legendary's `sync-saves`, GOG through gogdl's save sync.
- ROMs and standalone emulators (the runner's `.srm` and states, each emulator's save folder from
  the emulator catalog, the same owner as [save transfer](save-transfer.md)) sync to a target the
  player picks once: a folder another tool already syncs (Syncthing, Nextcloud, Dropbox), WebDAV,
  or another openSU on the LAN (the [LAN pairing](lan-remote-play.md)). Saves are keyed by
  console and game identity, not by path, so the same game on another OS finds its save.

## Order

1. One owner for "this game's saves" per runner and emulator (shared with save transfer). Built
   (2026-11-04): `library::saves::savesOf` lists each installed emulator's save slots of a ROM
   with size and newest change, keyed by `SaveKey` (console and Switch title id, or the ROM's name
   for the runner); standalone emulators' folders come from the catalog's `"saves"`, the runner's
   `.srm` and `.state` from `retro/save_paths`; an installed emulator that keeps no save of the
   game yet is listed too, as somewhere a save can go, its Flatpak's folders when it is one.
   Store games and the standalone emulators of other consoles have no save store yet.
2. LAN sharing first (the player, 2026-11-03: "LAN save sharing sounds great"): openSU machines
   that found and paired with each other ([LAN pairing](lan-remote-play.md), step 1) sync saves
   directly. Built (2026-11-04, `save_sync::SaveSync` with `lan::PeerSaveTarget`, [lan.md](../design/lan.md#shared-saves)): Share
   saves per paired openSU, off by default; the launched emulator's saves by `SaveKey`, pulled
   before a game starts and pushed after it ends; a change counter and content hash per game and
   the hash last synced with each peer; a save changed on both sides left for the player to pick
   on the details page; replaced saves backed up through `SaveCopier`; refused while the emulator
   runs; bodies sealed with ChaCha20-Poly1305 under per-request HKDF keys. Shown between two
   hidden openSUs on this PC over Avahi. Not shown between two machines or to the Windows VM:
   Windows builds no discovery (`dns_sd_none.cpp`) and the VM is on user-mode networking, so no
   openSU there can be found or reach this one.
3. Folder and WebDAV targets; Epic and GOG cloud through legendary and gogdl. Folder and WebDAV
   built (2026-11-04): `save_sync` is the engine for every target (restructured out of `lan`),
   the LAN's peers one group (`lan::PeerSaveTarget`), the share Settings › Saves names another
   (`save_sync::ShareTarget` over `FolderShare` or `WebDavShare`), with the same record, rule,
   staging, backups and conflict pick. A share holds `opensu-saves/key.json` (a salt and a sealed
   check) and each game's status and bundle sealed with ChaCha20-Poly1305 under the scrypt of the
   player's passphrase; the passphrase and the WebDAV password are in the secret store, the folder,
   address and user in `settings.json` (`saveShare`, `saveShareFolder`, `saveShareUrl`,
   `saveShareUser`). Shown with two hidden openSUs at once sharing one scratch folder, and the same
   against a stand-in WebDAV server in the project's Python environment
   (`scratch/saves3/share/{folder,webdav}/proof.txt`, frames in `frames/`): Kirby's runner save
   given by the first, taken by the second before its launch and loaded by the core; both changed,
   the second shows the conflict and keeps the share's after the pick, its own backed up; a third
   with another passphrase is refused; the bundle holds none of the save in the clear.
   Epic built (2026-11-04, `save_sync::EpicCloud`, routed by `app::SaveSharing` for an installed
   Epic game whose metadata names a cloud save folder, `Game::cloudSaves`; any other launches at
   once): the cloud's newest save is the one `legendary -v sync-saves <app>` with both transfers
   skipped compares with (its date as legendary reads it from the manifest's name), one request
   for that game, the save folder `legendary info --json --offline`'s `install.save_path` (set
   once by a `sync-saves` with both transfers skipped and `--accept-path`), and `epic-saves.json`
   keeps per game the folder's content hash and the manifest at the last sync, so the same rule
   decides; a pull is `sync-saves --force-download` after the folder is backed up, a push
   `--force-upload`, a conflict the same pick. Shown with two hidden openSUs and a stand-in `legendary` sharing one scratch
   cloud that records every call (`scratch/saves3/epic/proof.txt`, frames in `frames/`): the
   first gives its save, the second takes it before launch and gives its own after the game ends,
   the first, changed too, shows the conflict and keeps Epic's, its own backed up. Not run
   against Epic. Neither call refreshes legendary's game list, as `list-saves` and an online `info`
   do: a launch waits on one `info --offline` and one `sync-saves` request.
   GOG not built: gogdl's `save-sync <path> <client id> --ts <time>` needs per game the client id
   from `goggame-<id>.info`, the save locations from GOG's remote config
   (`cloudStorage.locations`) with their `<?SAVED_GAMES?>`-style folders resolved in the game's
   Wine prefix or the native home, and the timestamp it prints kept per location; it reports a
   conflict only as a log line. That is its own owner (`save_sync::GogCloud`) beside `EpicCloud`.
