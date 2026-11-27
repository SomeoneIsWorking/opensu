# The game sees the screen it is played on

Status: in progress (task #42, with the remote-play stutter).

A running game's display is always the screen the player is on now: this computer's monitor when
played here, the guest's screen when streamed. One owner decides which screen that is; the launch
arguments of the nested Gamescope, its live mode and the stream's encode size all read it.

- At launch: a game started for a guest starts at the guest's screen size, one started here at the
  monitor's.
- While it runs, the size and refresh follow every change: the local monitor's mode (docking, a
  mode change, another output), a guest's screen (rotation, docking, a desktop guest's window), a
  guest taking over or joining, and the player moving back here. The nested Gamescope takes the
  new mode live through our fork, so the game sees a resolution change rather than a scaled
  picture.
- The stream may be encoded below the guest's size when the measured link or encoder needs it; the
  guest upscales it (FSR 1).

## Asked

2026-11-26:

> Remote play stutter and it should also take into factor the guest resolution when launching the
> game or tell the game that the resolution is changed when... when the resolution changes... but
> that also applies to local
>
> So like remote screen and local screen change behavior remains the same, screen changes get
> detected, if guest screen then guest screen if host then host but the game always sees whatever
> monitor I'm playing on

## Proof

A local monitor change, a launch for a guest, a switch between here and a guest, and a guest's
screen change, each while a game runs, with the size the game reports following; and the remote
stutter's before and after numbers on the Ally.
