# Player profiles

Raised 2026-11-04: "Save sync can be optional because for example maybe two separate players play
on LAN, I haven't decided the exact logics, maybe it could be a profile system". The player
approved it and left its open questions to us; the defaults below are the spec.

## What it does

Saves follow a player, not a machine. Each openSU has one or more profiles (a name and a picture).
One is active: its saves are the ones a game loads, Copy saves copies and save sync shares. The
Guide menu's Profile row switches it, and openSU asks at start only once a second profile exists.
There is no new first-run step: an openSU starts with one profile, made silently, until the player
adds another in Settings › Profiles.

## Profiles

- `profiles.json` in the config dir (`profiles::ProfileBook`): each profile's id (16 random bytes as
  hex, like a LAN peer id), name, picture and links, and which one is active. The first profile is
  made, with the name "Player 1", the first time openSU starts with this code.
- A picture is one of eight colours, named on Settings › Profiles (`profiles::pictureName`).
- A profile is never removed in this plan; renaming and a new picture are Settings › Profiles rows.
- The profile picker at start lists the profiles by name, the last active one focused (Back keeps it);
  it does not show while only one profile exists. A hidden run (tools) never shows it and starts on
  the active profile.

## Saves per profile

- `library::saves::SaveKey` carries the profile id beside console and game, so every owner that
  reads, copies, backs up or syncs saves keys them per profile.
- The built-in runner keeps a profile's saves in `<saves>/profiles/<profile id>/<system>/`
  (`retro::profileSaveFolder`). The shell names that folder with `--saves` at each launch
  (`prepareRetroLaunch`), for the profile the game plays as, so the catalog's command names none.
- Copy saves' backups go under `<state>/save-backups/profiles/<profile id>/<system>/<game>/…`, so
  Undo copy restores the profile's own.
- `save-sync.json` keeps each game's counter and synced hashes per profile.

## Migration

Runs once at start, before anything reads saves (`profiles::moveIntoProfile`):

1. The first profile is made and `profiles.json` kept before anything moves, so its id is the same
   if the move is cut short.
2. Everything under `<saves>/` other than `profiles/` is copied into
   `<state>/save-backups/before-profiles/<UTC time>/`, made as `.part` and renamed when whole.
3. Each file is renamed into `<saves>/profiles/<first id>/` at the same relative path, then empty
   folders go. A file whose place is taken already stays where it is and is logged.
4. The same move takes `<state>/save-backups/<system>/` into the first profile's backups (no backup
   of the backups), and the entries of `save-sync.json` that name no profile become the first
   profile's.

Each step can be cut short and run again: a backup left as `.part` is made again, files already
moved are not there to move, and the record is rewritten whole. Once nothing outside `profiles/` is
left the move does nothing. A hidden run moves its own copies.

## Switch emulators

A Switch emulator keeps account saves per user (`nand/user/save/<user id>/…`, Ryujinx's by the user
in `ExtraData0`). A profile plays as one of the emulator's users by its place in openSU's profile
order: the first profile is the emulator's current user (`qt-config.ini`'s `current_user`,
Ryujinx's open profile), as before profiles; each further profile is the emulator's next user in
its own list (`profiles.dat`, `Profiles.json`), skipping the current one. Copy saves and save sync
read and write that user's account save (`SaveSlot::current` marks it). A profile the emulator has
no user for has no account save there: Copy saves says to add a user in the emulator. Device saves
are the console's, shared by every profile.

## LAN: profiles across paired openSUs

- `GET /lan/profiles` (paired, signed) lists this openSU's profiles: id, name, picture, and whether
  each is linked with the asker.
- Settings › LAN › Profiles on <peer> lists them. Copy asks over `POST /lan/profiles/copy`; the
  owner shows "<peer> asks for a copy of <name>" with Decline (focused) and Allow, the way it asks
  to pair, one question at a time, lapsing after 120 s. The asker polls
  `POST /lan/profiles/copy/status` and shows a card until the answer.
- Allowed, the owner records the link (profile id, peer id) and the asker makes the profile here
  with the same id, name and picture, linked with the owner. The new profile has no saves here, so
  the first sync of each game takes the owner's (no save on one side takes the other's).
- Only linked profiles' saves sync between machines: a peer's saves routes carry the profile id and
  are refused (403) for a profile not linked with that peer, and this side lists a peer as a target
  only for a linked profile. The Share saves switch per paired openSU goes: linking replaces it.
- A save sync carries the profile, so a peer's news (`/news`) syncs that profile here even when
  another is active.
- Unlink (Settings › Profiles, per link) drops the link here and tells the peer (`POST
  /lan/profiles/unlink`), which drops its side. Both keep the profile and the saves they hold, and
  each then syncs alone. Copying it again links them again; the saves rule (conflict when both
  changed) decides what each keeps.
- Removing a pairing drops its links.

## Remote play

A guest's `/lan/play` carries its active profile id. The host plays the game as its profile with
that id when it is linked with the guest, else as its first profile; the launch, its saves sync
before and after, and the runner's `--saves` all use that profile.

## Share and cloud

- Folder and WebDAV share: the active profile's saves sync with it. A game's files on the share are
  named from the profile id, console and game, so each profile keeps its own there and a profile
  meets only its linked copies on other openSUs. Files a share holds from before profiles are left
  where they are and no longer read; two openSUs that met only on a share meet again once one of
  them copies the other's profile over the LAN.
- Epic cloud: an Epic game keeps its saves where the game writes them, one folder per machine and
  Epic account, not per profile, so Epic's cloud syncs that folder whichever profile is active.

## Order of steps

1. This plan.
2. Done. Profiles and per-profile saves: `profiles::ProfileBook`, the migration, the runner's folder per
   profile at launch, `SaveKey` with the profile through Copy saves, backups, `save-sync.json`, the
   share's file names and the LAN's save routes (which refuse a profile that is not here, so
   between machines LAN saves pause until step 4 links profiles). Shown: a migration from an
   existing single-profile save tree in a hidden run.
3. Done. Settings › Profiles (add, rename, picture), the Guide's Profile row and the picker at
   start. A picture is shown by its colour's name; the picker's buttons are the names.
   Shown: two profiles on one machine keeping separate saves for the same ROM.
4. Done. LAN: the profile listing, Copy with the owner's question, links, linked-only sync, Unlink; the
   Share saves switch removed. Shown: copy and link between two hidden LAN openSUs with the question
   asked on the owner, and a linked profile's saves syncing while an unlinked one's do not.
5. Switch emulators' user per profile.
6. Remote play as the linked or first profile.
