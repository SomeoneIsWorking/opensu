# Link-cable multiplayer and netplay on the built-in runner

Requested 2026-11-05: GB and GBA link-cable games on openSU's built-in runner with the mGBA fork
that has splitscreen multiplayer and netplay (`SomeoneIsWorking/mgba`, upstream PR
libretro/mgba#318), split screen on one machine and netplay between two paired openSUs on the LAN,
each side playing as its own active profile. Later, the same netplay for any core and over the
internet with no server of ours.

## What the core is

PR #318's head, `splitscreen-new` (88d13c6c9), is the current branch: it carries the netplay commit
(rebased from `splitscreen-netplay` 7ae053304 as 35c3b115a), the 2026-09-09 fixes (syncArmed
removed, unsigned cycle compare, a save file per player) and libretro/mgba master as of 2026-09-09
(one commit behind it on 2026-11-05). The maintainers' objections: endrift would rather the core
coordinate multiplayer at a lower level in mGBA itself (planned for 0.12) than track Qt frontend
reimplementations; hizzlekizzle's testers saw stalls and links that never formed with `syncArmed`,
since removed; reports of Kirby Nightmare in Dream Land running at about 45 fps after doors and of
three players in a four-player session not linking. The author has stopped and suggested a
netplay-focused fork; nothing says it will merge.

How it plays:

- **One process runs every player.** `mgba_multiplayer_splitscreen` (Side by Side, Top/Bottom,
  4-Player Grid; read at load and reset) makes the core start N-1 more mGBA cores on the same ROM
  and join all N with a mutex-free copy of mGBA's GBA SIO lockstep (`libretro_lockstep.c`), run one
  after another on the frontend's thread (`mLibretroMultiplayerRunFrame`). GBA only:
  `_attachLockstep` refuses any core that is not `mPLATFORM_GBA`, so Game Boy link games do not
  link.
- **Input is per port.** `retro_run` reads ports 0 to 3 (`RETRO_DEVICE_JOYPAD`, bitmask when the
  frontend offers it) and player N is port N-1.
- **Video and sound.** One frame with every screen side by side (480x160 for two) when
  `mgba_multiplayer_av` is All; with Self, only the local player's screen and sound.
- **Saves.** Player 1's save RAM is the frontend's (`RETRO_MEMORY_SAVE_RAM`); players 2 to 4 are
  `<save dir>/<rom stem>.p<N>.srm`, opened and written by the core itself.
- **States.** One player: mGBA's own state, as the official core. Two or more: a header and every
  player's state with its save data and RTC (`libretro_savestate.c`).
- **Netplay is the frontend's.** The core has no network code and does not set
  `RETRO_ENVIRONMENT_SET_NETPACKET_INTERFACE`. Every machine runs all N GBAs; the frontend makes
  each machine see the same input on every port on the same frame (RetroArch's input-replicating
  netplay, delay or rollback). The core asks two things of it:
  - `RETRO_ENVIRONMENT_GET_NETPLAY_CLIENT_INDEX` (82 | experimental, from RetroArch PR #18776's
    `RARCH_NETPLAY_CTL_GET_SELF_CLIENT_NUM`): which player this machine is, read every frame, for
    Self's screen and sound. RetroArch answers with its client number; openSU answers with the
    local player's slot (port), which is what the core uses it as.
  - `RETRO_ENVIRONMENT_GET_SAVESTATE_CONTEXT`: only logged when it says rollback.

So what the frontend must give it: four joypad ports with distinct sources, the two calls above,
deterministic runs (same ROM, same core build, same options, same starting state on every
machine), and for rollback later, `retro_serialize`/`retro_unserialize` every frame.

## Decisions

- **The fork ships with openSU, built from a pinned revision.** Like librashader:
  `cmake/ThirdParty.cmake` pins `SomeoneIsWorking/mgba` at a commit on its `opensu` branch;
  `cmake/thirdparty/MgbaLink.cmake` builds `mgba_libretro` with mGBA's own CMake
  (`-DLIBMGBA_ONLY=ON -DBUILD_LIBRETRO=ON`, as libretro's CI does) into the shared prefix as
  `mgba_link_libretro`; `cmake/MgbaLink.cmake` stages it in the build tree and installs it under
  `<libdir>/opensu/cores/`, and the AppImage and APK carry it. `library/libretro_cores`'
  `CoreSearch` looks there after openSU's downloads. No prebuilt `.so` from anyone's checkout.
  Linux first; Windows, macOS and Android follow the toolchains the prefix already has.
- **The fork's branch `opensu`** starts at `splitscreen-new` 88d13c6c9. Only changes openSU needs
  go there, each its own commit; the player's branches are never rewritten.
- **Single play keeps the official core.** In one-player mode the fork runs the same emulation as
  libretro/mgba master (one commit behind, which is the one that quiets its per-transfer debug log)
  with the same state format; it is not better, and pinned it would fall behind the buildbot's
  nightly updates. It is picked only for a multiplayer launch.
- **Core-agnostic netplay.** The session and the transport know nothing of mGBA:
  - `netplay::Session` (new, `src/netplay/`): the peers and which player slot (port) each drives,
    each slot's input per frame, frame sync, the start state, a desync check and the end. mGBA is
    one client: it needs replicated input on its ports and its local slot. A core that sets
    `RETRO_ENVIRONMENT_SET_NETPACKET_INTERFACE` is another client later, its packets one more
    message kind on the same session.
  - `netplay::Transport` (interface): datagrams to and from the peer. The LAN one is
    `stream::UdpSocket` with `stream::PacketSeal`, keys from the pairing secret through
    `lan/pairing_keys` like remote play's. A WAN transport (an invite code carrying each side's
    candidates, STUN to learn the public address, UPnP or NAT-PMP to open a port, hole punching,
    no server of ours) is later and plugs in here.
- **Sync: delay-based lockstep first.** Each machine sends its slot's input for frame `f + d`
  while it runs frame `f`, each datagram repeating the last 16 frames so a loss costs nothing; a
  frame runs only when every slot's input for it is in, else that machine waits. `d` comes from
  the measured round trip (2 frames on a wired LAN). Every 120 frames each side sends the CRC-32
  of the core's system RAM (mGBA's states hold their creation time, so `retro_serialize` differs
  between machines in step); a mismatch ends the session with a desync message. Rollback (run ahead on
  predicted input, `retro_unserialize` and replay on a misprediction) is later and changes only
  the session.
- **Starting state and saves.** Both sides boot from the same files, so no state crosses the
  link: the ROM must match by SHA-256 (the guest can import it from the host first,
  `lan/rom_import`), and each side hands the other its active profile's `.srm` for the game before
  launch, over the signed LAN call. Both runners run in a session save folder under openSU's state
  folder, never a profile's, holding `<rom stem>.srm` (player 1) and `<rom stem>.p<N>.srm`; when the
  session ends each side copies back only its own player's save into its own profile.
- **Pairing and approval.** Only paired openSUs can ask (`/lan/netplay`, signed and sealed like
  `/lan/play`); the asked side's player is asked first, as for a profile copy, and may refuse.
- **Splitscreen input.** The runner gives each pad its own port in the order the pads first
  reach it (in the shell, the virtual pads of a Game claim, one per real pad); the keyboard is
  port 0. The control channel's pad gets a name (`POST /input` with `"pad"`), each named pad
  taking a port the same way, so a hidden run drives every player through the same path.

## Order

1. Plan, prior art, the link test ROM (`tools/link_rom`: Clang and lld for the ARM7TDMI, every
   player's colour sent in multiplayer mode and every screen drawing all four, a bar counting
   transfers). The unmodified fork links it in today's runner (both screens show red P1 and green
   P2).
2. The fork built from its pin through the shared prefix, staged, installed and found, with a
   catalog entry `mgba-link-core` for GBA (Linux).
3. Runner input per port: pads by first arrival, the control channel's named pads; four ports.
4. Splitscreen proof: a hidden runner, two named control pads, the link ROM side by side, A held
   on player 2 turning its band cyan on both screens; frames looked at.
5. Split `packet_seal` and `udp_socket` out of `stream` into a small library the runner can link
   without PipeWire and the codecs (its own commit). `netplay::Session`, `netplay::Transport` and
   the LAN transport; the runner's `--netplay` (the session's address, keys and slot from a file
   the shell writes), its remote ports from the session, `GET_NETPLAY_CLIENT_INDEX` answered with
   its slot, the desync check and the end when the peer goes quiet or says so. Shown first with two
   hidden runners on one machine.
6. `/lan/netplay` (ask, approve, keys, ROM check, save handed over, both sides launch) and the
   saves copied back. Shown with two hidden openSUs paired over Avahi: both screens' bands from
   both players, frame pacing and stalls measured, a state CRC compared, one side killed and the
   other ending cleanly.
7. UI: the details page's "Play linked" for GBA games (split screen) and the Guide's
   "Play with <paired openSU>" over a running GBA game; the fork's core picked for both.
8. Game Boy link (a fork change: mGBA's GB SIO lockstep in the core's cooperative scheduler),
   then Windows, macOS and Android builds of the fork.
9. Later, each its own plan: rollback in the session; any core's netplay through the same session
   (input replication works for every deterministic core; netpacket cores as a second client); a
   WAN transport.

## Progress

- Steps 1 to 4 landed. The splitscreen proof drove two named control-channel pads: a hidden runner
  reads no evdev pads, so uinput pads would not have reached it.
- Step 5 landed. Two hidden runners over loopback, hand-written session files: 59.7 fps on both,
  frame p50/p95/p99 16.7/16.8/16.8 ms, 2159 frames, 7 and 3 stalls, 17 memory checks matched; each
  side's A shows on both screens; SIGTERM ends the other at once, `kill -9` after 5 s, a different
  ROM at frame 120 on both.

## Risks

- A GBA cartridge with a real-time clock reads the host's clock and desyncs across machines;
  the start state carries the RTC, and the session would have to fix its source.
- The core's watchdog asserts (`mASSERT`) when every player blocks; a hang in a game ends the
  runner, which the shell reports as a crash.
- Three players in a four-player session do not link (reported on PR #318).
