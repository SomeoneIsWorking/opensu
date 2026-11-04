# Ideas

Maybes the player has raised but not decided. Nothing here is planned; a decided idea moves to
`docs/plans/`.

## Player profiles for shared saves

Raised 2026-11-04: "Save sync can be optional because for example maybe two separate players play
on LAN, I haven't decided the exact logics, maybe it could be a profile system".

Today LAN save sharing is per paired openSU: off by default, and on only when both sides turn on
"Share saves with <name>" (`docs/guide/lan.md`). That follows machines, not people, so two players
on two paired machines either share every save or none.

A profile system would make saves follow a player instead:

- Each openSU has one or more player profiles (name, picture); the active one is picked at start or
  from the Guide.
- Saves are kept per profile (`SaveKey` plus profile), and a profile's saves sync to the paired
  machines where that same profile exists, while another player's saves on those machines stay
  apart.
- Switch emulators already keep per-user save trees (`nand/user/save/<user id>/...`); a profile
  would map to an emulator user, so Copy saves and sync pick that user's tree.
- Linking, as the player sketched it ("You can see LAN profiles and copy them, in turn making it
  the 'same' profile aka linked"): a paired openSU lists the other machine's profiles; copying one
  creates it here with the same profile id and its saves, and from then on the two are one linked
  profile whose saves sync. A profile nobody copied stays local.
- Open questions: whether copying a profile needs that profile's own consent on the other machine
  (a PIN, or the owner accepting) or only the machine pairing; what unlinking does to the copies;
  whether the built-in runner's `.srm` files move into per-profile folders; and which profile a
  guest playing remotely uses.
