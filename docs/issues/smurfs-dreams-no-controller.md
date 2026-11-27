# The Smurfs – Dreams under Proton sees no controller

Status: fixed.

Reported by the player (2026-11-27, Ally, 55c76dd3): "Controller not working in this game", with
The Smurfs – Dreams (`Smurf_DreamsGOG.exe` from the games folder, GE-Proton11-6 through umu-run)
running from openSU.

The player: "I played this before from bazzite game mode just fine" (through Steam, which starts
Lutris game 24: GE-Proton10-34, prefix `~/.wineprefix`, which holds the saves).

Not the controller (2026-11-27, Ally): openSU's virtual pad gets every press, Proton's XInput has it
at index 0 and the game polls it, Wine has the game window in the foreground, and Enter sent to the
window does nothing either. The game ignores all input at "Press any button" in a prefix with no
`Smurf_DreamsGOG/Saved/SaveGames`, under GE-Proton11-6 and GE-Proton10-34 alike; with the saves
there it goes to its menu at once.

Cause: openSU runs a games-folder `.exe` in a new prefix of its own
(`<data>/wine-prefixes/own-folder-<id>`), never the one the player already plays it in, so the
player's saves are not there and this game cannot start a fresh profile.

Why that prefix: the games folder lists `Smurf_DreamsGOG.exe` as an own game, and openSU never
matched it to the Lutris game that runs the same `.exe`. It also could not have: this Lutris keeps
its game and runner YAML in `~/.config/lutris`, and openSU read them only from `~/.local/share/lutris`.

Fixed (2026-11-27): Lutris's YAML is read from `<config home>/lutris` once that folder exists
(`lutris_wine.cpp:foldersOf`), and a games-folder `.exe` a Lutris game runs runs as Lutris runs it
(`own_games.cpp:Provider::takeFromLutris`): here `lutris lutris:rungameid/24`, since Lutris pins
GE-Proton10-34 for it. On the Ally, launched through `/game/launch`, it reaches "Press any button"
and Enter opens its menu (Play, Options, Credits, Quit).
