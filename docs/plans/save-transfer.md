# Copying saves between emulators

Requested 2026-11-03: "things like for example Switch emulator, copy saves between emulators on
demand".

## What it does

A game's details page offers "Copy saves" when more than one emulator for its console has saves
for it: pick the source emulator, see each save's date and size on both sides, copy to the
target. The target's current saves are backed up first and the copy can be undone.

## Per console

- Switch: Ryujinx-line and yuzu-line emulators (Ryubing, Eden, Citron, sudachi) keep
  `nand/user/save/<user id>/<save id>` trees keyed by the title id; the user and save ids differ
  per emulator and are mapped through each one's own save index.
- libretro cores and standalone emulators of the same console: `.srm`/`.sav` and memory-card
  formats converted where they differ (size, header), as the runner already loads a smaller save.
- Each emulator's save location comes from the emulator catalog entry (`assets/emulators.json`),
  one owner for "where does this emulator keep this game's save".

## Order

1. Switch between two installed emulators, proven on the player's real saves with backups.
2. libretro and standalone pairs per console.
