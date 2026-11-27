# Choose the screen openSU plays on

Status: built on a desktop (Devices › Display, `host/screens`); not yet in openSU's own session.

Asked by the player (2026-11-27, Ally docked to a TV): "Repoint the ally to TV, this should also be
an option in openSU imo".

openSU offers the screens connected to the machine (the built-in panel, a TV or monitor on the dock)
and moves itself, and the game running in it, to the one the player picks: that screen turns on and
becomes the one openSU and its games are shown on, the others off. On a desktop the choice goes
through the desktop's own display settings (KScreen on KDE); in session mode openSU's Gamescope
takes the picked output. The picked screen's mode sets the nested Gamescope's size and refresh as
`game-sees-the-played-screen.md` describes.

## Built (2026-11-27)

On a KDE desktop, Devices › Display lists each connected screen ("Built-in screen", "DisplayPort
(DP-2)") with its mode, the one openSU is on "In use", the others "Use". Use runs `kscreen-doctor`
on the desktop's Wayland socket as a Wayland client: the picked screen on in the mode KDE last gave
it and made the main one, every other screen off; KWin moves openSU's Gamescope window with it.
Shown on the Ally docked to a TV, from the TV to the built-in screen and back.

Not yet: openSU's own session (a top-level Gamescope on DRM takes the screen it was started on), and
the running game's size and refresh following the switch (`game-sees-the-played-screen.md`); the
Output row keeps the refresh openSU started with.
