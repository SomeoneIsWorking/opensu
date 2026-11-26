# A Flatpak game's window is taken for another window: Guide does nothing in it

Status: open; cause found, fix in progress.

Reported by the player (2026-11-26, Ally, desktop mode, Sonic Robo Blast 2 from Flatpak
`org.srb2.SRB2`): "I tried playing Sonic Robo Blast 2 and the controller didn't work (probably the
game not configured which is fine) but then I couldn't exit the game since guide button didn't work
since it was a separate window"; then "Weird, controller works in it now, but guide doesn't".

## Evidence (Ally, `~/.local/state/opensu/opensu.log`)

Both launches log `focus on 0xffffffff80000001: another window` two seconds after `Sonic Robo
Blast 2 is running`, inside openSU's nested Gamescope. The running game's processes are
`/usr/bin/bwrap --args 95 -- release.sh` and `/app/bin/lsdlsrb2 ...`, in Flatpak's own
`app-flatpak-org.srb2.SRB2-*.scope`; `flatpak run` replaced itself with `bwrap`.

## Cause

`src/library/own_games.cpp:commandLaunch` gives a game from a `.desktop` entry the first word of
its Exec as the process hint, `flatpak` for every Flatpak game. No process of the running game
carries that word, so `FocusFollow` finds no game processes, holds the game's window to be another
window, frees the pads (`Claim::Away`) and leaves Guide to the window in front.

## Fix

The hint of a `flatpak run <app id>` command is the app id, which `ProcessTree::matching` already
finds through each process's `/.flatpak-info`.

