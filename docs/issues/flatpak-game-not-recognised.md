# A Flatpak game's window is taken for another window: Guide does nothing in it

Status: fixed (21195246, c3594444, 5f08099b, 228a1b6d, c8f7d099); shown on the Ally (2026-11-27).

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


## After the fix (2026-11-26, Ally)

With 21195246 the game's window is taken for the game (`focus on 0xffffffff80000001: the game`,
`Sonic Robo Blast 2 shows a window`). The player then: "controller doesn't work also in SRB2".
Open.

## Quitting openSU leaves the Flatpak game behind (2026-11-26, Ally)

The player: "Something weird happened, I used quit openSU while the game was open and the openSU
was gone but the game was still running and I hit guide again and I can hear openSU sounds but I
can't see it".

Also seen the same evening: `/game/close` answered "closed" while SRB2 kept running, twice. Close
and quit stop openSU's own `opensu-<pid>-game-N.scope`, and Flatpak moved the game into its own
`app-flatpak-<app id>-*.scope`, so the stop never reaches it.

The pad: on the Ally openSU has InputPlumber present the built-in pad as an Xbox 360 pad
(045e/028e), the same ids as openSU's virtual pad, so a game's
`SDL_GAMECONTROLLER_IGNORE_DEVICES_EXCEPT=0x045e/0x028e` lets both through and SDL lists the
grabbed real pad first: SRB2 had event17 (the grabbed pad) and event19 (the virtual pad) open.
With InputPlumber switched to `xbox-series` (045e/0b12) by hand, a fresh SRB2 opened only the
virtual pad.

## With c3594444 (2026-11-27, Ally)

The pad: InputPlumber presents `xbox-series` without a hand change, and SRB2 opened only
`opensu virtual pad` (event19, 045e/028e).

Close: openSU logged `pid 32481 left opensu-32140-game-0.scope; ending it and its children` and
`Sonic Robo Blast 2 closed` 25 ms later, but SRB2's game process was still running 16 s on. bwrap
exits at SIGTERM; the game under it ignores SIGTERM, and `SystemdScope::end` waited only for
bwrap, so nothing escalated. The escaped processes are now waited for and SIGKILLed at the stop
deadline.

## With c8f7d099 (2026-11-27, Ally)

The escaped processes get the scope's own five seconds (`TimeoutStopSec`) before SIGKILL. SRB2
ignores SIGTERM, so Close ends it in 5.1 s and Quit in 5.1 s, openSU itself gone 0.3 s after the
game. Quit with no game running ends openSU in 0.36 s.
