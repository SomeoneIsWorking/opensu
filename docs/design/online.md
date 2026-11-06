# Playing online

Two people play one game over the internet with no server of ours: no relay, no lobby. `src/online/`
gets two shells to agree on a game; the game itself then runs as a LAN session does, over
`netplay::UdpTransport` in `opensu-retro`. The reasoning is in `docs/plans/wan-netplay.md`.

## Owns

- `online::Meeting`: a thread from Play online until both sides agree, or fail. Input: `Network`
  (STUN servers, interfaces, the router) and `Patience`; output: a `View` for the screen, and an
  `Agreement` (port, peer, keys, saves, clock, delay).
- `invite_code`: what a code holds, read and written. `stun`: a Binding request. `nat_pmp`,
  `upnp_igd`: a router's mapping behind `PortMapper`. `port_lease`: that mapping kept and taken down.
  `rendezvous`: the sealed Hello, Welcome and Knock. `reliable_link`: Ask, Answer, Save and Go.
  `host_network`: interfaces, the default gateway, name lookups.
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
   `OnlinePlay` holds the `PortLease` until the game ends, however the runner ended.

## Invariants

- Nothing but a STUN answer is read unless it opens under a key from the code's secret, and a STUN
  answer must match its transaction id and come from the server asked.
- A socket and a mapping exist only from Play online to the end of the game.
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
Tailscale.

### Tailscale

With Tailscale on both machines nothing changes in openSU. Every interface's address goes in the
code, `100.64.0.0/10` first, so the same code reaches the host over the tailnet with no port opened.
Both people install Tailscale and share a machine or a tailnet, then play by code as usual.

## Checking it

`uv run --frozen python -m tools.nat_lab <case>|all` runs two hidden shells in rootless namespaces
across `nft` NATs with a STUN server and, for `nat-pmp` and `upnp`, a router (`gateway.py`), and keeps
frames and logs in `scratch/wan-netplay-proof/<case>/`. Each case that plays checks each player's
held A on both screens' bands, and that no code is in a log and no socket is left. Pad B is the
console's A: pads name buttons by place.

`settings.json`'s `onlineStunServers` (comma-separated `host:port`) replaces the default STUN
servers. `/state` gives `onlineStep`, and in a hidden run `onlineCode`.

`tests/online/` drives forged, replayed and oversized datagrams and wrong codes through the shipping
code.
