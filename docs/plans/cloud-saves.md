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
   directly, proven between this PC and the Windows VM.
3. Folder and WebDAV targets; Epic and GOG cloud through legendary and gogdl.
