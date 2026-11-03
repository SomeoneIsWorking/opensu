# Copying saves between emulators

Requested 2026-11-03: "things like for example Switch emulator, copy saves between emulators on
demand".

## What it does

A game's details page offers "Copy saves" when more than one emulator for its console has saves
for it: pick the source emulator, see each save's date and size on both sides, copy to the
target. The target's current saves are backed up first and the copy can be undone.

## Per console

- Switch: yuzu-line emulators (Eden, Citron, sudachi) keep
  `<nand>/user/save/0000000000000000/<user id>/<title id>`, the user id from `profiles.dat`'s UUID
  (second 64 bits first) and the current one from `qt-config.ini`'s `current_user`; a device save
  sits under the all-zero user. Ryubing keeps `<bis>/user/save/<save id>`, each with the save's
  attribute (program id, user id, type) in `ExtraData0` and its files in `0` (committed, what it
  mounts) and `1` (working, synced from `0`), the current profile the open one of
  `system/Profiles.json`; the player's own Ryubing build files them under
  `<bis>/user/save/<title id>/<user id>` instead, with the same `ExtraData0`. openSU finds a
  Ryujinx-line save by its `ExtraData0` either way (Ryubing writes it for every save at start), so
  it needs no save index of its own; a copy needs a save the target already made, since only the
  emulator writes its index.
- libretro cores and standalone emulators of the same console: `.srm`/`.sav` and memory-card
  formats converted where they differ (size, header), as the runner already loads a smaller save.
- Each emulator's save location comes from the emulator catalog entry (`assets/emulators.json`),
  one owner for "where does this emulator keep this game's save".

## Order

1. Switch between two installed emulators, proven on the player's real saves with backups. Built
   (2026-11-04): `library::saves` (`game_saves`, `switch_saves`, `save_copy`) and Copy saves on
   the details page (`SaveCopyController`); a copy is slot by slot for the kinds both keep
   (account save of the current profile, device save), the target's slots backed up whole under
   `<state dir>/save-backups/<system>/<title id>/<emulator>/<UTC time>/` and put back by Undo copy.
   Proven on scratch copies of the player's Eden and Ryujinx data (Mario + Rabbids Kingdom Battle,
   Eden to Ryujinx and undone byte for byte); Ryujinx is not installed here, so a stand-in on
   `PATH` stood for it. Not yet: copying into an emulator that has no save of the game, a guard
   against copying while the target emulator runs, Eden's "future" `user/save/account/...` layout
   (read by Eden, written by nothing yet), Flatpak save folders.
2. libretro and standalone pairs per console.
