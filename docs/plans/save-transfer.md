# Copying saves between emulators

Requested 2026-11-03: "things like for example Switch emulator, copy saves between emulators on
demand".

## What it does

A game's details page offers "Copy saves" when one installed emulator for its console has a save
of it and another can take one: pick the source (only emulators holding a save of the game), see
each side's date and size ("No save yet" on a target without one), copy to the target. The
target's current saves are backed up first and the copy can be undone; a target without a save
gets one made as that emulator would make it, and undo removes it again. Nothing is copied or
undone while either emulator runs.

## Per console

- Switch: yuzu-line emulators (Eden, Citron, sudachi) keep
  `<nand>/user/save/0000000000000000/<user id>/<title id>`, the user id from `profiles.dat`'s UUID
  (second 64 bits first) and the current one from `qt-config.ini`'s `current_user`; a device save
  sits under the all-zero user. Eden prefers its newer place where it is there
  (`savedata_factory.cpp` `GetFutureSaveDataPath`): `user/save/account/<UUID bytes, lower-case
  hex>/<title id>/0` and `user/save/device/<title id>/0`. A missing yuzu save is a folder at the
  old path, which Eden opens as it is. Ryubing keeps `<bis>/user/save/<save id>`, each with the
  save's attribute (program id, user id, type) in `ExtraData0` and its files in `0` (committed,
  what it mounts) and `1` (working, synced from `0`), the current profile the open one of
  `system/Profiles.json`. Kenji-NX (the player's fork; `GameSaveStorage`, `GameSavePath`, with
  `user/save-layout.lock` beside the saves) files them by game, `<bis>/user/save/<title id>/<user
  id>` and `<title id>/device`, and rebuilds its index from each folder's `ExtraData0` at start.
  openSU finds a Ryujinx-line save by its `ExtraData0` either way. A missing one is made as
  LibHac makes it: `ExtraData0` and `ExtraData1` (the attribute, the owner and sizes from the
  game's `control.nacp`, a fresh commit id), `.lock`, `0` and `1`; under Kenji-NX in its game
  folder, under Ryubing with the next save id after `lastPublishedId` and an entry in the index
  (`bis/system/save/8000000000000000/{0,1}/imkvdb.arc`, a KeyValueArchive of SaveDataAttribute to
  SaveDataIndexerValue kept in LibHac's attribute order), whose save is backed up with the copy.
- A Flatpak install keeps the same folders under `~/.var/app/<app id>`, `~/.config` as `config` and
  `~/.local/share` as `data` (the sandbox's XDG folders), so Eden's Flatpak keeps its NAND in
  `~/.var/app/dev.eden_emu.eden/data/eden/nand`.
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
   Proven on scratch copies of the player's Eden and Ryujinx data; Ryujinx is not installed here,
   so a stand-in on `PATH` stood for it. Then (2026-11-04): only an emulator holding a save is a
   source; a target without one gets it made (yuzu's folder, Kenji-NX's game folder, Ryubing's
   save id and index entry) and undo removes it; a copy or undo is refused while either emulator
   runs (`launch::ProcessTree`: a command line holding the program or AppImage, or a Flatpak
   sandbox whose `/.flatpak-info` names the app id, which its command lines do not);
   reads, copies and undos run on the card's worker with the row showing how far; Flatpak save
   folders and Eden's newer layout are read. Proven in hidden runs on scratch copies (Donkey Kong
   Country: Tropical Freeze made in Ryujinx and undone, under Kenji-NX's and Ryubing's layouts;
   Mario Kart 8 Deluxe refused with a stand-in Ryujinx running, then copied Ryujinx to Eden and
   undone; Kingdom Battle grown to 40,000 files for the progress row; Kingdom Battle from a
   Flatpak Eden's newer layout, refused first while a stand-in Eden sandbox ran).
2. libretro and standalone pairs per console.
