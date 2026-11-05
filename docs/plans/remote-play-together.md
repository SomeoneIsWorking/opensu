# Remote play together

Requested 2026-11-05: "Remote play AND remote play together". Remote play streams one host's game
to one guest (`docs/design/lan.md#remote-play`). Play together streams one host game to several
paired guests at once, each guest's controller its own player in the game, as Steam's Remote Play
Together does.

## What it does

- A host's game, started on the host or for a guest over remote play, takes up to four guests.
- Each guest sees the host's picture and hears its sound, and plays with its own pad as its own
  player. The host's own pads stay where they were.
- A guest joins by asking (Play on a game the host is playing, which the host's player allows) or
  by an invitation (the host's Guide › Invite to play, which the guest's player accepts).
- The host's Guide › Players shows who is connected, with Kick.

## Decisions

### Inviting

Both ways, and the host's player approves every guest either way, as pairing and profile copies
do (`HostPairing`, `ProfileCopies`: one question at a time, Decline focused, lapsing after 120 s).

- **A guest asks.** Play on a host's game that the host is running (on its own or with guests)
  goes over `/lan/play` as now. Instead of the 409 "a game is already running there", the host
  answers `waiting` and asks its player "<guest> asks to join <game>" over whatever it shows; the
  guest polls `/lan/play/status` every second and shows "Waiting for <host>". Allowed, the status
  gives the stream's port. A different game running there is still refused, saying which.
- **The host invites.** Guide › Invite to play, while a game runs, lists the paired, announced
  openSUs not in the game. Picking one sends `/lan/invite`; the guest's player is asked "<host>
  invites you to play <game>" (Decline focused). Join plays it as above, and the host lets the
  guest in without asking again: the invitation was its player's approval. An invitation is for
  that peer and game only and lapses after 120 s.
- A game started for a guest over remote play (the first guest) is let in without a question, as
  today: that guest asked for the game, and pairing approved it.

### Players

- One `gamepad::RemotePad` per guest, made when the guest is let in, gone when it leaves. Its
  virtual pad is named as the host's own (`opensu virtual pad`, phys `opensu/virtual-pad`), so it
  is never taken for a physical pad, by the shell or by anyone reading `/proc/bus/input/devices`,
  and every game knows its Xbox 360 identity.
- Slots are the order the game sees pads arrive. The host's own pads have virtual pads from the
  game's launch, so each guest's pad, made later, comes after them, in join order. A game started
  for a guest has that guest's pad made before the launch, so in that one game the first guest is
  player one.
- The built-in runner gives every pad its own port: the shell starts it with `--players 5`, the
  player here and four guests, so `retro::Joypad` gives each pad the lowest free port as it is
  first heard from; the keyboard plays port 0. A pad leaving frees its port and every other pad
  keeps its own, so a guest leaving or kicked does not move anyone.
- The keyboard and pointer go to the game only from the guest a game was started for. Every
  other guest plays its pad alone: there is one X display and one pointer, and two guests' held
  keys would fight in it.

### Streams

Each guest keeps its own `HostStream`: its UDP port, its keys, its `LinkRate` with its probes and
reports, and its pacer. What is coded is shared.

Measured on this machine (Radeon RX 6700 XT, 1280x720 at 60 frames, `scratch/pacing`, the
`encode` stage of `remoteVideo` in five runs): PyroWave codes a frame in 1.4 to 1.9 ms at the
median and 1.8 to 3.0 ms at p95; HEVC on VA-API in 3.2 to 3.5 ms and 3.6 to 5.0 ms. A frame lasts
16.7 ms, and the capture thread codes it before the next arrives.

- **One encode for everyone** at the slowest guest's plan would put every guest on the hardware
  codec at 2 to 40 Mbit/s as soon as one guest's link cannot carry 100 Mbit/s with room, where a
  wired guest gets PyroWave at up to 200 Mbit/s and no key frames. On a hardware codec a loss on
  any one guest's link makes it ask for a key frame (`KeyFrame`), so a lossy Wi-Fi guest would
  push a key frame, five times a normal frame, at every other guest too.
- **An encode per guest** keeps every guest at its own plan, but four guests on HEVC are four
  hardware encodes, 13 to 20 ms at p95 one after another on the capture thread, which passes the
  frame.
- **Chosen: one encode per codec in use.** The guests whose plan is PyroWave share one PyroWave
  encode at the lowest of their rates; the guests on a hardware codec share one encode of that
  codec at the lowest of theirs. A slow guest gets its own hardware encode and the fast ones keep
  PyroWave: two encodes, about 5 ms at the median and 8 ms at p95, the usual case with a slow
  guest. A key frame asked for goes to that codec's guests only, and a guest joining a codec's
  group starts it on a key frame. The frame number is the capture's, the same in every group, so
  `/remote/frame.png` compares by number on every guest.

Measured with two guests on the same machine (720p60 NES test ROM, `remoteVideo` p50/p95): the
PyroWave group's encode took 0.90/1.01 ms, 0.91/1.16 ms with HEVC alongside; the capped guest's
HEVC stage, which counts the PyroWave encode coded before it, 4.73/6.63 ms. That is inside the
5 ms and 8 ms estimated, and a third of a frame at worst.

### Audio

One capture and one Opus encode, sent to every guest. A game started for a guest plays into the
session's own sink, as today. A game the host plays itself already plays on the host's output, and
moving it would silence the host's speakers, so the session captures that output's monitor
instead: what the host hears, the guests hear.

### Saves

The game plays as the profile it was started as: the host's active one for the host's own game,
the first guest's linked one (else the host's first) for a game started over remote play. A guest
who joined syncs nothing: no save sync runs for it on either side, and the host's saves before and
after are the profile's alone.

### Limits

- At most four guests (`maxGuests`). A fifth's ask is refused with "<game> has four guests".
- One host game at a time, as now; a guest plays one remote game at a time.
- Guide › Players on the host lists each guest in join order with Kick, which asks first. Kick ends
  that guest's stream ("<host> removed you"), and its pad goes.
- The game ending ends every guest's stream. A guest leaving frees its pad and the game goes on; a
  game started for a guest closes when its last guest leaves, a host's own game never does.
- A guest's Guide reads Leave game when it joined (the game goes on there) and End remote play when
  the game was started for it.

## Owners

- `stream::Audience` (new): the guests' streams a session sends to, under one lock, so the video
  and sound never send to a stream being removed.
- `stream::HostVideo`: one capture; an encoder per codec in use across the audience.
- `stream::HostAudio`: one capture and encode to the audience; `stream::GameSound` captures the
  default output's monitor when it is given no sink of its own.
- `stream::HostInput`: the pad, and the keys and pointer only when given an X display.
- `app::prepareRetroLaunch`: the runner started with a port for the player here and each guest.
- `lan::wire`, `lan::LanListener`, `lan::LanNode`: `waiting` on `/lan/play`, `/lan/play/status`,
  `/lan/invite`, the guest's poll.
- `app::RemotePlay`: the host's session with its guests, asks and invitations, Kick; the guest's
  invitation and whether it joined.
- `app::TogetherQuestions` (new): the host's question for a guest's ask and the guest's for an
  invitation, on the navigator.
- `app::GuideMenuController`: Invite to play and Players.

## Order

1. Two guests, one host: the audience, an encode per codec, shared sound, a pad per guest, the
   runner's ports, the guest's ask with the host's question. Shown with two hidden guests against
   one host inside a headless Gamescope on a two-player NES test ROM in the built-in runner: both
   guests' frames matching the host's by number, one guest capped onto HEVC while the other stays
   on PyroWave, each guest's pad in `/proc/bus/input/devices` and lighting its own half of the
   game's screen.
2. Invitations, the Guide's Invite to play and Players with Kick, a game the host plays itself
   (its output's monitor), Leave game; every dialog and Guide page shown in frames.
</content>
</invoke>
