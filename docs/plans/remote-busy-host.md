# Remote play by profile

Asked 2026-11-18: "fedora already running Legend of Zelda ... you are completely locked out ...
show running game, resume it / take over from remote etc or close it options", then: "The goal is
to give seamless remote experience to same profile on another device as if they are using
locally. An extra scenario is remote play together where another profile can join the game. And
this should even work when both the same profile and the other profile are remote ... I think
things should be profile based like that and people on the same LAN but different profiles are
also considered friends as they can discover your game and ask to join". Think of Steam's remote
play: nobody need be at the computer that runs the game.

## The rules

- A profile is a person; a device is only a screen. A profile linked with another on a paired
  openSU is the same person there.
- **The same profile, any device: as if it sat there.** Asking for the game it plays continues it
  here, the stream moving and the game going on where it is; closing it or playing another game
  closes it, saved first on a built-in core. Nobody is asked, on either side.
- **Another profile is a friend.** It sees the game playing there ("Playing <game>") and may ask
  to join it as the next player. The profile playing it is asked, on the device it plays on now:
  the computer's own screen, or the device it streams to. Let in, the friend plays from its own
  device. A friend cannot close or take over another profile's game.
- Both may be remote: the player on a phone streaming the game, the friend on a tablet asking.
- **Friends on the LAN.** A friend is anyone on the LAN with another profile, whose openSU can find
  the game and ask to join, without the two devices first being paired.

## State

- Built: the launcher says "Playing <game>" (`library::playingOn`, `Launcher::playing`).
- Built: asking for another game on a busy openSU asks on the device asking (`app/busy_host`). The
  same profile: Continue it here, Close it and play <game>, Cancel. Another profile: Ask to join it,
  Cancel.
- Built: the same profile continues it with no question (`RemotePlay::continueSeat`).
- Built: the same profile closes it with no question (`/lan/close`, `app/close_asks`); another
  profile is refused at the route (`LanListener::closeRoute`, 403) and never reaches the close.
- Built: the join question follows the player. While the player plays the game from another
  device (the seat of the player there, or of the guest a game was started for), the host passes a
  friend's ask to it (`RemotePlay::passAsks`, `/lan/join/ask`), which asks there (`PassedAsks`,
  `TogetherQuestions`) and answers back (`/lan/join/answer`, `RemotePlay::answeredThere`); nothing
  shows on the computer's own screen. An ask that cannot be passed, or whose player came back, is
  asked there. Shown between two nodes (`lan_node_test`); a session with a stream needs a Gamescope
  host, which the LAN lab does not have yet.
- Not built: friends without pairing. Today an openSU sees another's games only once the two are
  paired.

## Owners

- `lan/wire`: `CloseRequest`; the join question sent to the device playing and its answer.
- `app/close_asks`: the same profile's close.
- `app/remote_play`: takes a seat over, asks the profile playing to let a friend in.
- `app/together_questions`: asks on the computer's own screen; on the guest's when the player is
  remote.
- `app/busy_host`: the question on the asking device.

## Order

1. The rules above for the same profile, and the busy question. Shown with two hidden openSUs
   (`tools.nat_lab lan-busy`): another profile is offered Ask to join; after copying the host's
   profile, the same profile closes the host's game with nothing asked there.
2. The join question on the device the player plays on: the host passes a friend's ask to the
   guest holding the player's seat, which answers it. Left: a lab rig whose host runs in a nested
   Gamescope, to walk it with three openSUs.
3. Friends without pairing: a LAN openSU lists the games playing on others and asks to join one.
