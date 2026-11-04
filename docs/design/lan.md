# LAN

The LAN integration (Settings › Integrations › LAN, off by default) makes this openSU findable by
the other openSUs on the network, pairs with them, and lists the installed games of each paired
one in Library under that openSU's name. Starting such a game says "Remote play is not built yet";
streaming is step 2 of `docs/plans/lan-remote-play.md`. Module rows are in `docs/codemap.md` under
LAN.

## Owners and data flow

- `app::IntegrationSwitch` turns `app::LanService` on and off with the integration and rebuilds
  the catalog, which lists `lan::remoteProvider` while LAN is on.
- `app::LanService` owns `lan::LanNode` while LAN is on. Turning LAN off, or renaming this
  openSU, ends the node: its listener, announcement, pairing and worker stop, and the remote games
  leave the catalog. Each frame it shows the host's question and the guest's card as the pairings
  stand, and passes the node's news on: changed remote games reload the catalog, changed peers
  refresh Settings.
- `lan::LanNode` holds the `PeerBook`, the `LanListener`, the DNS-SD announcement and browsing,
  `HostPairing`, the `GuestPairing` under way, the `SharedLibrary` (the installed local games,
  given by `ShellApp::applyCatalog` after covers are applied) and a worker. The worker looks every
  2 s at who is announced and asks each paired, announced peer for its listing and any cover it
  does not hold yet. It asks again when the set changes, when a pairing changes, after a peer that
  did not answer (2 s, then doubling up to 60 s), and every 60 s. Covers are kept under
  `<cache>/opensu/lan/<peer id>/<token>.<ext>`; those of peers no longer paired are deleted.
- `lan::RemoteLibrary` holds the remote games (`Source::Remote`, id `lan:<peer>:<game>`, the host's
  id and name, the cached cover, `installed`, no launch). `library::libraryShelf` gives each host
  a launcher (`launcher:lan:<peer id>`), sorted by name, after Your games. Home leaves remote games
  out. The details page shows "On <host>", Play, Game settings and Hide game.

## Discovery

Each node announces `_opensu._tcp` on its listener's port with TXT `id` (its 32-hex peer id),
`name` (at most 63 bytes, no control bytes) and `v` (protocol `1`). On Linux this goes through
Avahi's D-Bus API (`EntryGroupNew`/`AddService`/`Commit`, `ServiceBrowserNew`, `ResolveService`)
over sd-bus on the node's own thread. A name clash takes Avahi's alternative name. Only IPv4
resolutions are kept, and its own id is skipped. An openSU on the same machine resolves on every
interface, loopback included; the first one is listed. Windows, macOS and Android build
`dns_sd_none.cpp`, and Settings › LAN says finding openSUs is not built there yet.

## Wire

Plain HTTP/1.1 on the LAN listener (`lucent::http::Server`, `ListenScope::LocalNetwork`, every
IPv4 interface, 16 KiB bodies), port `OPENSU_LAN_PORT` (default 7312, a free one in a hidden run).
It never shares the loopback control channel's port.

| Route | Who | What |
| --- | --- | --- |
| `GET /lan/hello` | anyone | `{"id","name","v"}`: all an unpaired openSU learns |
| `POST /lan/pair` | anyone | `{"id","name","commitment"}` → `{"key","nonce"}` (host's X25519 key, 16-byte nonce); 409 while another guest pairs |
| `POST /lan/pair/reveal` | anyone | `{"id","key","nonce"}`; 409 unless it matches the commitment |
| `POST /lan/pair/status` | anyone | `{"id"}` → `{"verdict","signature"}`: `waiting`, `accepted` or `declined` |
| `GET /lan/library` | paired, signed | `[{"id","title","cover"}]`, installed local games only |
| `GET /lan/cover/<token>` | paired, signed | one listed cover, at most 16 MiB |
| `POST /lan/unpair` | paired, signed | the caller forgets this openSU; this one forgets it |

Everything else, and any signed route that fails its check, is 403.

## Pairing

1. The guest makes a fresh X25519 key `Kg` and nonce `Ng` and sends
   `commitment = SHA-256("opensu-lan-commit\n" || Kg || Ng)`.
2. The host answers with its fresh `Kh` and `Nh`. The guest reveals `Kg`, `Ng`, and the host
   checks them against the commitment.
3. Both hold the transcript (ids, both keys, both nonces) and show the same code, six digits from
   its SHA-256 (`123 456`). The host asks its player, Decline focused. The guest's card shows the
   code until the answer comes or 120 s pass.
4. Accepted, both derive `secret = HKDF-SHA-256(X25519(Kg, Kh), salt = Ng || Nh, info = ids and
   keys)`; it never crosses the wire. The host keeps it first, then answers. Its verdict is
   `HMAC(secret, "opensu-lan-verdict\n" + verdict + "\n" + hex(Ng))`, and the guest keeps the host
   only when the signature matches.

The guest commits before it sees the host's key, so a man in the middle has to fix his half of
each transcript first. The two codes then differ, except for a one-in-a-million guess. One secret
serves both directions, so each paired openSU lists the other's games.

## Signed requests

A signed request carries `X-OpenSU-Peer` (the caller's id), `X-OpenSU-Time` (Unix seconds),
`X-OpenSU-Nonce` (16 random bytes, hex) and `X-OpenSU-Signature`:
`HMAC(secret, "opensu-lan-request\n" METHOD "\n" TARGET "\n" time "\n" nonce "\n" hex(SHA-256(body)))`.
The host takes it only from a paired id, within 120 s of its own clock, with a nonce not seen from
that peer within that window (`ReplayGuard`), and compares the signature in constant time. Its
answer carries `X-OpenSU-Signature` =
`HMAC(secret, "opensu-lan-answer\n" nonce "\n" status "\n" hex(SHA-256(body)))`, and the client
drops an answer that does not match. Redirects are not followed.

## Keys and secrets

`lan.json` (config dir) holds this openSU's peer id and each paired peer's id and name. Each
secret is in `secrets::SecretStore` as `lan-peer-<id>`: the desktop keyring when it is there, else
an owner-only file under `<config>/secrets/`. A hidden run uses its own copies and files only.
Secrets are wiped from memory after use (`secrets::wipeBytes`) and never logged.

Remove (Settings › LAN) forgets the peer here at once, and tells it through a signed
`/lan/unpair` when it is announced. After that, its requests are refused and its games leave
Library on both sides.

## Known limits

- No confidentiality. The listing, titles and covers cross the LAN in clear and can be read by
  anyone on it. Only who may ask and whether an answer is genuine are protected.
- Anyone on the LAN can see that an openSU runs, its name and id, and can ask to pair. The host's
  player sees each request, one at a time.
- A request is good for 120 s of clock skew, and the replay guard forgets nonces after that.
  Machines whose clocks differ by more cannot talk.
- A peer told nothing (it was off the LAN when removed) keeps its half until it is removed there
  too. Its requests are refused here.
- A cover fetched for a local game after the catalog was read is shared from the next catalog
  read.
- Discovery is Linux only. The rest is portable C++ over libcrypto and Lucent.

## Checking it

`tests/lan/lan_node_test.cpp` runs two nodes and a stranger in one process on loopback, with a
stand-in directory in place of Avahi. In the product, start two or three `--hidden` openSUs, each
with its own `HOME` and the XDG dirs under it and `"integrations": {"lan": true}` in its
settings. Drive Settings › LAN through each control channel. `/state` reports `lanSeen`,
`lanPaired` and `lanCode`, `/frame.png` shows the code on both screens, and the `lan` log lines
name every pairing, listing and refusal. A request to `/lan/library` without a pairing gets 403.
