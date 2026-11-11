# Netplay over the internet

Requested 2026-11-06: friends play a game together over the internet, RetroArch-style, with no
server of ours: no relay, no lobby. Two stages, each landed on its own.

- **Stage 1, join by code.** Play online on a game gives a code to send a friend; the friend enters
  it and both play through the session the LAN already has.
- **Stage 2, any core.** The same session for any libretro core in the runner, one console on each
  machine, rollback where the core can.

## What a code has to carry

A server would tell each side where the other is. Without one, the code does that, and a chat app
is the only channel, so what matters is how many times a person has to carry something across it.

A hole in a NAT opens only from the inside. A home NAT with the usual address-dependent filtering
drops the friend's datagrams until the host has sent to the friend's address, which the host cannot
know from a code it wrote before the friend looked. So **one code cannot work everywhere**:

| Host's network | What reaches it | Codes needed |
|---|---|---|
| Router opened a port (UPnP, NAT-PMP) | anyone, at the mapped port | one |
| Full-cone NAT, or no NAT | anyone, at the STUN address | one |
| Same LAN, or both on Tailscale or ZeroTier | anyone, at a local address | one |
| Port-restricted NAT, the common case | only addresses it sent to | two: the host must learn the friend's address |
| Symmetric on one side, restricted on the other | nothing without port guessing | fails |
| Symmetric on both | nothing | fails |

Options weighed:

1. **Two codes always** (ICE's offer and answer). Works wherever punching does, but makes every
   game two trips even when the router opened a port.
2. **One code, no reply.** Simple, and fails for most home networks that have no UPnP.
3. **A public rendezvous** (BitTorrent DHT, an MQTT broker, Nostr). One code everywhere, but a
   third party in the path that we neither run nor can answer for, a dependency that can vanish,
   and it was agreed there is no lobby. Rejected; but see [Embedded ZeroTier](#embedded-zerotier),
   which takes a third party for the last resort only.
4. **Invite, then a reply only if it is needed.** The chosen one.

The friend pastes the invite. openSU tries the host at once. If that lands, which it does in the
first three rows of the table, both screens move on and there is one code. If nothing lands within
three seconds the friend's screen shows a reply code to send back; the host pastes it and both
punch. For two people on a chat app the rule is short: *send your code; if your friend gets a reply
code, paste it.* Nobody has to know what kind of router they own, and the reply field sits on the
host's screen from the start.

The address in a code is only good while the NAT remembers it, and people take minutes. Each side
keeps its mapping alive with a STUN Binding request every 15 s for as long as a code is out, and an
invite lapses after 10 minutes.

### What is in a code

Crockford base32, no separators so a double click selects it, and whitespace, dashes and case are
ignored on the way in, so what a chat app does to a long line does not matter. `OSU` starts an
invite and `OSR` a reply, so the wrong one is named as such.

- Invite: version, whether the mapping varies by destination, the 16-byte secret, the local port,
  up to 3 public addresses (STUN's, the router's mapping), up to 3 local addresses, a 2-byte check.
  56 characters for one of each.
- Reply: version, the same flag, the local port, the addresses, 8 bytes of HMAC under the invite's
  secret. A reply to another invite is refused by name. It carries no secret.

Interface addresses go in, Tailscale's and ZeroTier's first (`docs/design/online.md`), so with
either on both machines the same code reaches the host over it, with no ports opened and nothing
else to learn. A local address tells the friend the host's private address and
nothing more; the host is told that its address is shared before the code is made.

## Embedded ZeroTier

2026-11-11, at the player's request: ZeroTier should need no setup. That reverses option 3's "no
third party in the path" for one case only, when no direct path works. Without it, two symmetric
NATs, or a symmetric one against a restricted one, cannot play at all unless both people install
and set up Tailscale or ZeroTier themselves; the player chose a relay they do not run over that.

- Lucent's online transport (`lucent::p2p`) gets an embedded ZeroTier backend on libzt, which joins
  a controller-less ad-hoc network (`ff` + the port twice + zeros, no account, no install); the
  code becomes Lucent's descriptor, carrying the host's node id and that port.
- The friend tries the direct paths first; ZeroTier's roots, which relay when ZeroTier's own
  punching fails, are only on the path when nothing else worked.
- The ad-hoc network is public. Its traffic is taken only after the same Hello and Welcome under
  the code's secret, so anyone else on it gets nothing that opens.
- It is a library linked into openSU and its runner (the process that plays owns the node), not a
  runner, so G001's rule is not touched.

Licence, read through `gh api` on 2026-11-11: libzt `main` at `a707ea6a` (2024-11-07) is under the
Business Source License 1.1, Change Date 2026-01-01, Change License Apache 2.0; its ZeroTierOne
submodule at `c53c6bd9` is BSL 1.1 (version 1.4.4's text), Change Date 2025-01-01, Apache 2.0. Both
dates have passed, so that code is Apache 2.0 now; lwIP is BSD. Lucent's pinned revision is checked
again against its own `LICENSE.txt` files before openSU consumes it.

## Why not libjuice

Read at v1.7.4 through `gh api` (`agent.c`, `conn_mux.c`, `ice.c`, `juice.h`). It is good code and
does not fit, for reasons in its shape rather than its quality:

- **The socket.** An agent owns its socket and the game runs in another process (`opensu-retro`).
  What must survive from the rendezvous to the game is a local port, which our `UdpSocket` hands
  over by closing it and letting the runner bind it. An agent would have to live in the runner,
  which has no UI to take a code from, or be torn down, leaving it with nothing to do after the
  connection is made.
- **The wait.** `ICE_PAC_TIMEOUT` fails an agent after 39.5 s, and a person with a chat app takes
  minutes. The agent would have to start only once the second code is in, which loses the one-code
  case: ICE needs both sides' `ufrag` and `pwd` before anything passes.
- **What it does not do.** No UPnP or NAT-PMP, and one STUN server, so it cannot tell a symmetric
  NAT, which is what lets us say so plainly. It has a TURN path we would have to leave switched off.
- **Two sets of crypto.** Its checks are STUN with HMAC-SHA1 under credentials sent in clear beside
  ours. The promise here is that what does not open under the seal is dropped.

What is left to write is a STUN Binding client (one request, one attribute) and a loop of sealed
datagrams to a few addresses. That is less code than the glue, and no new dependency on three
platforms. **So no libjuice, and nothing new in the prefix.** It also does not go in Lucent: it
rests on `stream::UdpSocket`, `stream::PacketSeal` and `secrets`, which are openSU's. If a second
product wants it, it moves with them.

UPnP and NAT-PMP are written here too. NAT-PMP is a 12-byte request. UPnP is SSDP over a `UdpSocket`
and two SOAP calls over `net::WebClient`, with bodies capped and the router's URL held to the
address that answered.

## Design

```
shell (online::Meeting, a thread)                  runner (opensu-retro --netplay)
  STUN x2, PortLease                                 UdpTransport on the same local port
  Rendezvous: Hello/Welcome/Knock on one UdpSocket      netplay::Session
  ReliableLink: ask, answer, saves, go
  closes the socket ------ same port, same NAT mapping ------> binds it
  PortLease renews until the game ends (app::OnlinePlay holds it)
```

`src/online/` is the shell's alone, so the runner links none of it.

- `netplay::UdpTransport` (was `LanTransport`) is the transport for both paths. A session over the
  internet differs in how the peer is found, not in what a datagram is. It now follows the peer's
  address on any datagram that opens, as a NAT can rebind a port between the rendezvous and the
  game.
- The rendezvous and the shell's exchange run in the shell, on the socket that made the code. To
  start the game the shell closes it and the runner binds the same port: the NAT's mapping is keyed
  on the inside address and port, so it carries over. The session file is the LAN's (`port`, `peer`,
  keys, `delay`); the runner gains nothing for stage 1.
- `online::PortLease` holds a router mapping and lives in the shell, not the runner, so a crashed
  runner still has the mapping taken down.
- `app::NetplayRun` (split out of `LanNetplay`) writes the session folder and file, launches, and
  puts the save back; `LanNetplay` and `OnlinePlay` both feed it.

### The handshake

The key for the first messages is HKDF of the code's secret. It is the same on every attempt with a
code, so these use a fresh random 96-bit nonce, as the LAN's sealed bodies do, never a counter.

1. Friend to host, `Hello`: its ephemeral X25519 key, sealed with ChaCha20-Poly1305.
2. Host to friend, `Welcome`: its ephemeral key and the friend's, sealed the same way.
3. Both derive the session's keys: HKDF over the X25519 secret and the code's secret, salted with
   both public keys, one key per sender and purpose (link, game).
4. Friend to host: the first `stream::PacketSeal` datagram under those keys. Only now does the host
   take that address as its peer, burn the code and ignore everything else.

`Knock` is the host's punch, sealed like a `Hello`; the friend adds its source address to where it
sends, which is what lets a symmetric host reach a full-cone friend.

Punching has an order. The lab showed the friend's datagrams reaching the host's router before the
host has sent anything leave a conntrack entry there, and the host's first datagram to that address
then clashes with it, so a Linux NAT gives it another public port and the friend's NAT drops it. So
the friend leaves the port of its direct try behind when it shows a reply code: a new socket, a new
STUN answer, and the code carries those. While it waits it sends only 3 hops (`nearHops`), which
holds its own NAT open and dies before the host's. The host's `Knock` then passes that hole, and the
friend answers in full through the one the host just opened. A path of 3 hops or fewer, or more than
two NATs on the friend's side, would defeat this.

Because of the X25519 step, reading the code later, or holding it and a recording of the traffic,
does not open the session. Holding the code *during* the session lets someone race the friend, as
with any bearer invite; the code is one-time, so the friend then simply fails to join and the host
sees someone else on it. That is what "keep the code to your friend" means in the UI.

### After the connection

`online::ReliableLink` carries ordered messages over the sealed datagrams, with acks, a window,
and keepalives that also give the round trip. There are four kinds of message and nothing else,
which is the whole of "game data only": `Ask` (the ROM's hash, size, console, the facts), `Answer`,
`Save` and `Go` (the clock and the delay). Nothing a peer sends is a path, and the ROM is looked up
in the local catalog by console, size and hash, so its file name does not have to match. Each
message's declared size is checked against its kind's cap before anything is allocated.

The friend is asked before anything of theirs leaves: the question names the local game found by
hash, and it says the save goes across. Facts are compared as on the LAN (`lan::netplayDifference`),
so a differing core revision, BIOS or option is refused the same way.

`delay` comes from the measured round trip: enough frames to cover half of it plus a frame, at
least 2 and at most 12. The host picks it, as both must agree.

### Failure, in words

Each side measures whether its mapping varies by destination, by asking two STUN servers from one
socket. Both varying, with no router mapping on either side, cannot work, and the friend is told after
its direct try; a host learns it from the reply code. A symmetric NAT against a restricted one looks fine to
both until punching stops: the host gives up 15 s after a reply code goes in, and the friend 2
minutes after the invite. All of it ends in the same message, which says there is no relay and
names Tailscale and ZeroTier. No waiting state is without a limit, and every one can be cancelled.

## Security promises and where each is held

| Promise | Held by | Tested by |
|---|---|---|
| No open ports outside a session | The socket exists only from Play online to the end of the game. `PortLease` removes the mapping when it goes; `OnlinePlay` holds it, so a dead runner still takes it down | `port_lease_test`, the lab's gateway log |
| Short lease on a crash | 120 s, renewed at half. A router offering only permanent mappings (UPnP 725) gets none from us: punching is used instead | `port_lease_test`; `kill -9` in the lab |
| One-time secret, unauthenticated dropped | Everything but STUN opens under a key from the secret, or is dropped uncounted. STUN answers must match a transaction id and come from the server asked | `rendezvous_test` |
| Secret never logged | Nothing logs a code, a key or a datagram; a code is only on the screen and clipboard | The lab greps both logs for the secret |
| Same encryption as the LAN seal | `stream::PacketSeal` and `secrets::crypto` | `rendezvous_test`, `reliable_link_test` |
| Game data only | Four message kinds; no field is a path | `reliable_link_test` |
| States size-checked | `netplay::StateSync`, against the local core's own size before any allocation, then its hash, and none at all when both machines hold the same state | `state_sync_test` |
| The friend sees your IP | The question before a code is made or entered | Frame in the proof folder |

## Stage 2: any core

`library/linked_play` gives a game's core for a session; any core the runner can run may play. What
must match grows from the link core's pin to: the SHA-256 of the core's library, each BIOS file it
boots from, and its options.

The session runs one of two ways, chosen when the session starts and the same on both machines:

- **Delay lockstep**, as now, for a core that cannot serialize or says so
  (`RETRO_SERIALIZATION_QUIRK_INCOMPLETE`, `_MUST_INITIALIZE`, `_CORE_VARIABLE_SIZE`,
  `_SINGLE_SESSION`, `_ENDIAN_DEPENDENT`, `_PLATFORM_DEPENDENT`), for one that draws on the GPU, and
  for the link core.
- **Rollback**: the session lets a machine run ahead on a repeat of the other's last input. The
  link keeps a state per frame in a ring, and when real input differs from what was guessed it
  unserializes and replays with the sound and picture off. `retro_serialize` is called with
  `RETRO_ENVIRONMENT_GET_SAVESTATE_CONTEXT` answering `ROLLBACK_NETPLAY`, so a core leaves out what
  it may.

A core's boot can differ between machines. Where the core can roll back, player 1 offers the size
and SHA-256 of its state at frame 0, and a machine holding that same state takes nothing, so a core
that boots alike has no peer's state near it. Otherwise that state is the only one that crosses the
wire, and `netplay::StateSync` checks it: the declared size against the core's own
`retro_serialize_size` before anything is allocated, then its SHA-256, and only then
`retro_unserialize`. Neither says the state is well meant: a friend holding the code could send a
hostile one that hashes, and only the core's own checks stand behind that. The context is
`ROLLBACK_NETPLAY` for all of a session's states, which the header defines as ones that may be
loaded by another binary on another machine.

Rollback's point is less input delay, so the host's `Go` gives the least delay and the rest of the
round trip as `ahead`, at least 4 frames. A core that cannot roll back adds `ahead` to its delay.

One console on each machine has one save: both run the host's, the friend's stays out of it, and
only the host keeps what the game writes.

Desync detection stays, on system RAM, or on a state's CRC where a core exposes no RAM. In rollback,
checks are only sent for confirmed frames. The LAN's Play with <name> stays on the link core.

## Order

Stage 1: this plan; rename to `UdpTransport`; `stun`, `invite_code`, `host_network`; `port_lease`
with NAT-PMP and UPnP; `rendezvous`, `reliable_link`; `app::NetplayRun`, `app::OnlinePlay` and its
screens; `tools/nat_lab`; proof, push, CI.

Stage 2: core facts; session modes and rollback; `state_sync`; runner; UI; proof, push, CI.

## Not proven here

Two real networks: real NATs behave worse than `nft` ones, and hairpinning, CGNAT and routers that
lie about UPnP are only reasoned about. Windows and macOS build in CI but cannot be run in the lab.
Rollback was shown on the mGBA core (Game Boy through the shell, GBA with two runners alone) and the
test core. Lockstep for a core with quirks, and a GPU core, were only reasoned about. IPv6 is left out: `UdpSocket` is IPv4, and it would make many of these cases simply work, so it is
the first thing to do if punching proves weak in practice.
