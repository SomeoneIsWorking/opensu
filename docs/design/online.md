# Playing online

Two people play one game over the internet with no server of ours: no relay, no lobby. Lucent's
transport (`lucent::p2p`, `lucent/docs/p2p.md`) takes a code to an authenticated path; `src/online/`
gets two shells to agree on a game over it; the game itself then runs as a LAN session does, over
`netplay::UdpTransport` in `opensu-retro`. The reasoning is in `docs/plans/wan-netplay.md`.

## Owns

- `online::Meeting`: a thread from Play online until both sides agree, or fail. Input: `Network`
  (Lucent's `p2p::DirectNetwork`: STUN servers, interfaces, the router) and `Patience` (Lucent's,
  plus `agreeing`); output: a `View` for the screen, and an `Agreement` (port, peer, the game keys,
  saves, clock, delay). It runs Lucent's `p2p::Connector` to a path, then Ask, Answer, Save and Go
  over Lucent's `ReliableLink` with openSU's caps. It owns openSU's code prefixes (`OSU`, `OSR`), its
  key label (`opensu-online`) and the words for each failure and code problem.
- Lucent owns the rest: codes (`p2p::Descriptor`), STUN, NAT-PMP, UPnP and the lease, which
  addresses a code keeps (`p2p::reachable_locals`), the sealed Hello, Welcome and Knock
  (`p2p::Handshake`) and the session's keys (`p2p::SessionKeys`: `link` and `game`, one per
  sender).
- `app::OnlinePlay` and `app::OnlineScreen`: the shell's side, on its loop. `app::NetplayRun`: the
  session folder, the launch and the save put back, for LAN and online alike.

## Data flow

1. Play online on a game page, or Settings › Play online › Join, says the friend will see this
   machine's public address. Only then is anything sent.
2. The host binds a UDP port, asks two STUN servers from it, asks the router for a mapping
   (NAT-PMP, then UPnP) and writes the code (`OSU...`). STUN keeps the NAT's mapping alive every 15 s.
3. The friend pastes it and tries each of the host's addresses for 3 s. With no answer it takes a new
   port, shows a reply code (`OSR...`) to send back, and sends only 3 hops out, so its NAT stays open
   and nothing reaches the host's router first. The host pastes the reply and knocks; the friend
   answers in full. Reaching a Linux NAT first would move the host's public port.
4. `Hello`, `Welcome`, and the first `PacketSeal` datagram spend the code. The friend is asked about
   the game found by hash, facts are compared as on the LAN, saves cross, and the host sends `Go`.
5. The shell closes its socket and the runner binds the same port, so the NAT's mapping carries over.
   A game found over ZeroTier keeps its path in the shell instead, as only the shell's node can carry
   it: the runner plays to a Lucent `PathBridge` on loopback, which `OnlinePlay` holds with the lease.
   `OnlinePlay` holds the `PortLease` until the game ends, however the runner ended.

## Invariants

- Nothing but a STUN answer is read unless it opens under a key from the code's secret, and a STUN
  answer must match its transaction id and come from the server asked.
- A socket and a mapping exist only from Play online to the end of the game. The ZeroTier node,
  once Play online started it, stays until openSU quits: libzt cannot start a node again in the
  same process.
- No code, key or datagram is logged. A code is on the screen and the clipboard only.
- Four kinds of message cross, each with a cap checked against its declared size before anything is
  allocated. None holds a path; the ROM is looked up by console, size and hash.
- Every wait has a limit (`Patience`) and can be cancelled.
- A mapping asked for 120 s is renewed at half that. A router that gives more than 600 s, or only
  permanent mappings, gets none from us.

## When it cannot connect

Symmetric NATs on both sides are named after the direct try. A symmetric NAT against a restricted one
looks fine until punching stops: the host gives up 15 s after a reply goes in, the friend 2 minutes
after the invite. All of it ends in `Failure::Unreachable`, which says there is no relay and names
Tailscale and ZeroTier.

### Tailscale and ZeroTier

With Tailscale or ZeroTier on both machines nothing changes in openSU, which neither ships nor
installs either: it uses the interface the player's own install made. A code holds at most 3 local
addresses (Lucent's `p2p::kMostAddresses`), and `p2p::reachable_locals` picks them:

1. Interfaces without a carrier are left out: Docker's and libvirt's bridges sit up with none while
   nothing runs on them, and would otherwise take the slots.
2. Link-local addresses (`169.254.0.0/16`) are left out: they reach no one past the cable.
3. Tailscale's (`100.64.0.0/10`) and ZeroTier's come first, as they are the fallback; then the
   interface the default gateway is on; then the rest, in the system's order.

ZeroTier is known by what it names its interface: `zt` and a hash of the network on Linux, `feth` on
macOS, and the adapter description "ZeroTier Virtual Port" on Windows. A Linux interface the player
renamed (`devicemap`) is not recognised and is ranked as any other, so it still goes in when the
slots allow. The address itself says nothing: ZeroTier's pools are ordinary private ranges.

Both people install Tailscale and share a machine or a tailnet, or install ZeroTier and join one
network, then play by code as usual. On Windows, ZeroTier's network is a Public one until the player
makes it Private, and Windows Defender Firewall then blocks openSU's port on it.

#### Trying it over a real ZeroTier network

The lab proves the path with a stand-in interface; a real network is the player's to make:

1. Install ZeroTier on both machines (zerotier.com/download) and sign in at my.zerotier.com.
2. Create a network, join it from both (`zerotier-cli join <network id>`), and authorise both
   members in the network's page.
3. Check each machine has an address on it (`ip -br addr` shows a `zt...` interface).
4. On the host, Play online on a game: its log's `[p2p] ... local (...)` line names the `zt`
   address first.
5. The friend pastes the code: both connect with no reply code needed, as over a LAN.
6. With LAN on in both, Settings › LAN lists the other openSU found over ZeroTier (`[lan] found
   <name> at <zt address>`), and pairing works as on a LAN.

### Embedded ZeroTier

So that the fallback needs no install and no account, openSU carries a ZeroTier node of its own:
Lucent's `lucent::zerotier` on libzt (`cmake/thirdparty/Libzt.cmake`, licences in
`docs/prior-art.md`), owned by `online::ZeroTier` (`src/online/zero_tier.*`). This puts ZeroTier's
roots in the path when no direct path works, which `docs/plans/wan-netplay.md` had ruled out; the
reasons it is taken now are there. What stands:

- The node starts when the player first opens Play online (host or join), keeps its identity in
  `<data>/zerotier/` (`identity.secret` is never logged), and joins openSU's one ad-hoc network,
  `online::zeroTierPort`. A host waits up to Lucent's `Patience::zerotier` for it to be there, so
  the code carries its node; with warm nodes a pasted code connects in about 2 s. A code that
  carries a node does not warn of a strict router: a strict one cannot stop ZeroTier.
- The friend dials both at once: the direct candidates and the host's node on that network. The
  first handshake to finish wins; a friend behind a NAT that varies, against a host behind one,
  keeps trying ZeroTier instead of failing.
- An ad-hoc network is open to anyone who picks its port, so nothing that arrives over it is taken
  unless it opens under the code's secret: one handshake for every backend (Lucent's ZeroTier test
  runs a stranger with the host's node and network but not the code, which never connects).
- `settings.json`'s `onlineZeroTier` (default on) turns it off; `onlineZeroTierRoots` names a root
  set file in place of ZeroTier's roots (the lab's). Windows builds have none yet
  (`OPENSU_BUILD_ZEROTIER` off: libzt's Windows build needs a fork fix) and say so in Settings.
- libzt is a library linked into openSU, not a runner, so G001's rule against bundled runners does
  not apply to it.

## Any core

A game on the link core plays as on the LAN. Any other game the built-in runner runs is one console
on each machine (`library::netplayCore`), each player's input on its port, and asks for rollback
(`docs/design/libretro.md`): the host's `Go` gives the least delay and what is left of the round trip
as frames to run ahead, and a core that cannot roll back takes those frames as delay instead.
Both machines run the host's save and only the host keeps what the game writes; the friend is told so
before Play. Being the same core means the same SHA-256 of its library (`wire::coreBuildPrefix`), of
the BIOS files it boots from, and the same options; a difference is refused by name, so two people
need the same build of a core, and different systems are refused.

Play with <name> on the LAN stays on the link core.

## Checking it

`uv run --frozen python -m tools.nat_lab <case>|all` runs two hidden shells in rootless namespaces
across `nft` NATs with a STUN server and, for `nat-pmp` and `upnp`, a router (`gateway.py`), and keeps
frames and logs in `scratch/wan-netplay-proof/<case>/`. Each case that plays checks each player's
held A on both screens' bands, and that no code is in a log and no socket is left. Pad B is the
console's A: pads name buttons by place. Every shell plays on mGBA (built-in) as openSU downloads
it (`core_cache.py`): a hidden openSU outside the namespaces downloads the catalog's pinned release
once into `scratch/nat-lab-cores/<release hash>/`, and each shell gets a copy. `any-core` plays a
generated Game Boy ROM on it at 120 ms of round trip and asks for frames run again.
`zerotier` puts symmetric NATs on both sides, a ZeroTier stand-in (`zt0`, `10.147.17.0/24`, not the
default route) between the machines and two idle bridges on the host (`docker0`, `virbr0`), and
plays by one code; `zerotier-lan` gives each machine its own system bus and Avahi (`mdns.py`) and
pairs the two over LAN discovery on `zt0`. Both capture `zt0` in `wire-zt0.log`.
`zerotier-embedded` puts symmetric NATs on both sides and nothing between the machines but openSU's
own node, through a lab root on a machine of its own past the internet node
(`tests/online/zero_tier_root.cpp`; ZeroTier binds each address to its interface, so the root's
address is on the link it is reached by); both logs must say `connected over ZeroTier`, no node's
private key may be in a log, and the sockets left are only the node's, bound to an interface. Every other case plays with
`onlineZeroTier` off, so the symmetric cases still prove the direct path alone fails.

`settings.json`'s `onlineStunServers` (comma-separated `host:port`) replaces the default STUN
servers. `/state` gives `onlineStep`, and in a hidden run `onlineCode`.

`tests/online/` drives forged, replayed and oversized datagrams and wrong codes through the shipping
code.
