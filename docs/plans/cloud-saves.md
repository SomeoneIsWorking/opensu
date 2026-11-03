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

1. One owner for "this game's saves" per runner and emulator (shared with save transfer).
2. Folder and WebDAV targets for ROM saves, proven between this PC and the Windows VM.
3. Epic and GOG cloud through legendary and gogdl; LAN peers once pairing exists.
