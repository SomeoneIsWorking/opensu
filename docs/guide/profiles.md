# Profiles

A profile is one player on this openSU: a name, a colour and their own saves. Games load and keep
the saves of the profile you play as, Copy saves copies them, and save sharing shares them.

openSU starts with one profile, *Player 1*, and asks nothing until you add another.

## Settings › Profiles

- **Name**: how the profile is shown here and on the LAN. A opens a card to type it.
- **Picture**: the profile's colour; Left and Right step through eight.
- **Play as** *name*: makes it the profile games play as from now on. The one you play as reads
  *Playing now*.
- **Add profile**: names a new profile, with saves of its own, empty until it plays.
- **Linked with** *name*: a profile copied to or from another openSU on the LAN. Its saves stay
  the same on both. **Unlink** asks first, then both keep it as a profile of their own with the
  saves each has, no longer synced.

## Profiles on the LAN

Settings › LAN › Profiles on *name* lists another paired openSU's profiles. **Copy** asks it for
one: the other openSU asks its player, Decline first, as pairing does. Allowed, the profile is
copied here with its saves and linked with that openSU; from then on its saves follow whoever
played it last ([lan.md](lan.md#shared-saves)). A profile not linked with it keeps its saves to
this openSU. Removing the pairing unlinks every profile linked with it.

## Switching

- The Guide menu's **Profile** row, beside Volume and Interface size: Left and Right step to the
  next profile. It is refused while a game runs; close the game first.
- With two or more profiles, openSU asks *Who is playing?* when it starts, the last one played as
  focused. B keeps it.

## Where the saves are

The built-in runner keeps each profile's saves in
`~/.local/share/opensu/saves/profiles/<profile id>/<console>/`, and Copy saves backs up under
`~/.local/state/opensu/save-backups/profiles/<profile id>/`. The saves you had before profiles
became *Player 1*'s the first time this openSU started with profiles, backed up first under
`~/.local/state/opensu/save-backups/before-profiles/<time>/`.
