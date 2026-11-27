# The Smurfs – Dreams is titled "The Smurfs – Dreams – Cosmic Skin"

Status: fixed.

Reported by the player (2026-11-27, Ally): "Yea I was wondering why it said Cosmic Skin", of the
games-folder game `~/Games/The Smurfs - Dreams/Smurf_DreamsGOG.exe`, which openSU's tile, loading
card and Quick settings name "The Smurfs – Dreams – Cosmic Skin", a DLC's name, not the game's.

Cause: the game's folder holds a `goggame-<id>.info` for the game and one for each DLC (Cosmic Skin,
Digital Deluxe Upgrade), and `game_identify.cpp:gogName` took whichever the folder listed first.

Fixed (2026-11-27): `gogName` takes the info whose `gameId` is its `rootGameId`, the game itself.
On the Ally its details page reads "The Smurfs – Dreams", with its cover.
