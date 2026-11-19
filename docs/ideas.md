# Ideas

Maybes the player has raised but not decided. Nothing here is planned; a decided idea moves to
`docs/plans/`.

- Run Steam games through fumarole instead of Isolated Steam, so a game is openSU's own child: no
  kept virtual pads for the sandbox, the sound sink's environment reaches it, and no client to
  hide. Windows games under Wine only until fumarole has a native `steamclient.so`; Steam Input
  and anti-cheat games would stay on the client.
- Local Steam games on Android, which needs fumarole: the Steam client doesn't run there.
- A touch layout for shooters that is all keyboard and mouse, with no pad, so a game's prompts
  never flip between the two: a swipe moves the mouse, since a right stick turns worse (Risk of
  Rain 2), and each on-screen control is a key or mouse button. Needs a virtual keyboard and mouse
  on the host, and a key for each control per game.
- A QR code beside the Play online invite and reply codes, so a friend can take a code off a photo
  or another screen without reading it (`docs/issues/invite-code-legibility.md`).

## Friends on the LAN without pairing

Asked 2026-11-18 and put off for a better friend system later: anyone on the LAN with another
profile finds a game playing and asks to join it without pairing first. With it, a checkbox at
launch to play a game without sharing it, so nobody finds it or asks to join. A first sketch, kept
for then:

What an unpaired openSU learns today is `GET /lan/hello` (id, name) and nothing else; every other
route needs a pairing, whose keys sign the requests and seal the stream. The design keeps that and
folds pairing into the join:

- The announcement's TXT record carries the title of the game playing there, if any, and whether
  it takes guests (`playing`, `joinable`): no ids, no profile, nothing an unpaired one can act on.
  An openSU that does not want to be seen playing turns it off in Settings › LAN.
- An unpaired openSU lists it on Home as "<name> is playing <game>", with **Ask to join**.
- Ask to join runs the pairing handshake (`/lan/pair`, the commitment and reveal) with a join
  flag and the game. The host does not show the pairing question: it shows "Let <name> join
  <game>?" to the player, wherever they play it (step 2's passing). Let in pairs the two as
  pairing would and admits the guest; Decline leaves them unpaired.
- A pairing made this way is a guest pairing: it lists nothing, shares no saves, ROMs or profiles,
  and ends with the session unless the player keeps it from the Players list in the Guide.

Its risk is the TXT record: anyone on the LAN can read what a computer plays, so it is on by
default only while a game takes guests.

## Libretro ports of standalone emulators

A libretro core of Eden (Switch) or Cemu (Wii U) would let the built-in runner own input, pacing,
shaders, presets and the Guide for those consoles, as it does for the cores it hosts. Neither exists;
each is a port of a large C++ emulator onto the libretro API (its renderer onto the runner's Vulkan
context, its threads onto `retro_run`), kept in a fork pinned by revision. Until then
`docs/plans/emulator-launch-config.md` sets the standalone builds up per launch.

## Native ports of console games

Fan-made native PC ports built from decompilations exist for some titles; openSU could offer one as
another way to run a game it lists, beside the emulator, on the player's own game data. What exists
for Super Mario Galaxy is in `docs/prior-art.md` ("Native ports of console games").

## Game identity from IDs across stores and ROMs

Asked 2026-11-19 and put off: the title rule (`library::gameTitles`) still links a GOG or Epic copy to
its Steam copy, and a ROM to its game. What would replace it where IDs exist:

- Stores: Wikidata, which needs no key and is CC0, links one game's Steam (P1733), GOG (P2725) and
  Epic (P6278) ids; asked per unmatched GOG or Epic game and kept, so a GOG or Epic copy becomes the
  Steam app it is (`steamAppOf`) and groups by it. IGDB was the other choice and was not taken: it
  needs a Twitch developer key on every machine.
- ROMs: the CRC or serial against the libretro database openSU already downloads, for the canonical
  name, in place of the file name's.

Why it waits: title grouping's real mistakes are fixed (an added game takes art only from its own
Steam app; two Steam apps never group), and a store copy whose title differs from its Steam copy's is
rare.
