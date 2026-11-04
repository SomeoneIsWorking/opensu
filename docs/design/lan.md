# LAN

The LAN integration (Settings › Integrations › LAN, off by default) makes this openSU findable by
the other openSUs on the network, pairs with them, lists the installed games of each paired one in
Library under that openSU's name, shares the games' saves with the ones the player chose, and
plays such a game remotely: it runs on the host and streams here (see [Remote play](#remote-play)).
Module rows are in `docs/codemap.md` under LAN and Remote play.

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
- `lan::SaveSync` (in the node) shares saves; see [Shared saves](#shared-saves).
- `app::RemotePlay` (on the shell's loop, both ways) runs remote play's sessions; see
  [Remote play](#remote-play).

## Discovery

Each node announces `_opensu._tcp` on its listener's port with TXT `id` (its 32-hex peer id),
`name` (at most 63 bytes, no control bytes) and `v` (protocol `2`: sealed bodies). On Linux this goes through
Avahi's D-Bus API (`EntryGroupNew`/`AddService`/`Commit`, `ServiceBrowserNew`, `ResolveService`)
over sd-bus on the node's own thread. A name clash takes Avahi's alternative name. Only IPv4
resolutions are kept, and its own id is skipped. An openSU on the same machine resolves on every
interface, loopback included; the first one is listed. Windows, macOS and Android build
`dns_sd_none.cpp`, and Settings › LAN says finding openSUs is not built there yet.

## Wire

HTTP/1.1 on the LAN listener (`lucent::http::Server`, `ListenScope::LocalNetwork`, every
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
| `POST /lan/saves/status` | sharing saves, signed | `{"system","game"}` → `{"counter","hash","bytes","modified"}`; 404 when no game here has that key |
| `POST /lan/saves/get` | sharing saves, signed | the same ask → the saves as a bundle (below), at most 512 MiB |
| `POST /lan/saves/news` | sharing saves, signed | the caller holds newer saves of the game (`picked`: the player there chose them in a conflict); this one pulls them |
| `POST /lan/saves/synced` | sharing saves, signed | the caller took `hash` (`counter`) of the game from here |
| `POST /lan/play` | paired, signed | `{"game","salt"}` → `{"port","salt"}`: the host streams that listed game to the caller; 404 for a game it does not list, 409 with why when it will not (busy, outside Gamescope, no capture) |

Everything else, and any signed route that fails its check or whose body does not open, is 403.
The saves routes are 403 too, sealed, while this openSU does not share saves with the caller.

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

The code is a short authentication string over both keys, so it binds the key exchange only if a
man in the middle gets few guesses. In protocol 1 the host answered a new `/lan/pair` at once
after a refused reveal, so he could keep his commitment to the guest, open fresh offers to the
host without revealing, and reveal only to the one whose code matched the guest's. The host now
takes a new offer at most every 10 s (`offerSpacing`) until its player has answered one: about
12 tries in the guest's 120 s, each one in a million.

## Signed and sealed requests

A signed request carries `X-OpenSU-Peer` (the caller's id), `X-OpenSU-Time` (Unix seconds),
`X-OpenSU-Nonce` (16 random bytes, hex) and `X-OpenSU-Signature`:
`HMAC(secret, "opensu-lan-request\n" METHOD "\n" TARGET "\n" time "\n" nonce "\n" hex(SHA-256(body)))`,
over the body as sent (sealed). The host takes it only from a paired id, within 120 s of its own
clock, with a nonce not seen from that peer within that window (`ReplayGuard`), and compares the
signature in constant time. Redirects are not followed.

Every body of a signed call is sealed with ChaCha20-Poly1305 (libcrypto's EVP), a POST's request
body and every answer, refusals included; a GET's request body is empty. Each request and direction
has its own key:

- `key = HKDF-SHA-256(secret, salt = the request nonce's 16 bytes, info)`, with
  `info = "opensu-lan-seal\n" ("request" | "answer") "\n" METHOD "\n" TARGET "\n" time`.
- The additional data is `info "\n" status` (0 for a request), so an answer's status is
  authenticated with its body.
- The sealed body is a random 12-byte AEAD nonce, then the ciphertext and the 16-byte tag. The key
  is already unique to the request; the random nonce keeps a key from sealing two answers under one
  nonce when a request is replayed after the host restarted and its replay guard is empty.
- The host opens a request body only after the signature and replay checks, so each nonce reaches
  sealing once per run. The client drops an answer that does not open, which replaces the answer
  HMAC protocol 1 had: only the holder of the secret could seal it, for this request.

The headers (route, peer id, time, nonce, `Content-Type: application/octet-stream`, lengths) cross
in clear. The secret is long-lived: someone who later learns it can open recorded traffic (no
forward secrecy).

## Shared saves

Settings › LAN has a Share saves switch per paired openSU, off by default (`"shareSaves"` in
`lan.json`). Saves cross only while both sides have it on and the peer is announced.

- Which saves: those of the emulator the game starts with (`library::saves::launchedSaves`), the
  slots a copy carries (`copiedSlots`), matched across openSUs by `SaveKey` (console and Switch
  title id, or the ROM's name for the runner). The saves taken are copied over the ones here by
  `library::saves::SaveCopier`, so a Switch save made by Eden on one side lands in Ryujinx's
  format on the other through the same code as Copy saves; other consoles' saves go runner to
  runner.
- The record (`lan-saves.json`, state dir): per game a change counter, raised whenever the content
  hash here changes, the hash (SHA-256 over each file's slot kind, path and bytes; times do not
  count), and per peer the hash both held when they last synced.
- The rule (`syncStateOf`): the same hash is in sync; no save on one side takes the other's; else
  a side whose hash differs from the last-synced one changed. One side changed: its saves go to
  the other. Both changed, or two openSUs that never synced hold different saves: conflict, and
  nothing is replaced until the player picks.
- When: before a game starts (`LanService::before` holds the launch on the shell's loop, "Syncing
  X's saves first", until `SaveSync::prepared`); after it ends; from Sync saves now; on opening its
  details (look only); when a peer says it has newer saves (`/news`) or took these (`/synced`).
- A pull asks `/status`, takes the bundle from `/get`, checks its hash against the status, stages
  it under `<state>/lan-incoming/<peer>/`, checks that the saves here did not change meanwhile and
  are not in use (the game the shell plays, or the emulator's process through
  `launch::runningRefusal`), and copies it in, the saves replaced backed up first under
  `<state>/save-backups/`. It then records the hash and tells the peer (`/synced`).
- A push is only `/news`: the peer pulls, under its own checks. Request bodies stay small.
- In a conflict the details row is Pick which save to keep: a dialog with each side's date and
  size. Keep this one tells the peer with `picked` set, and it pulls over its own; Keep X's pulls.
- The work runs on `SaveSync`'s thread, one game at a time; no lock is held while a peer is asked,
  so two openSUs syncing each other cannot deadlock. A hidden run keeps the record, staging and
  backups in its own folder (`PlayerFiles::stateDir`).

The Saves fact on the details page shows the peer most worth attention: Conflict with X, Newer on
X, the reason a sync failed, Newer here than on X, X does not share saves with this openSU, X is
offline, In sync with X.

## Remote play

Play on another openSU's game (`ShellApp::launch`, `Source::Remote`) goes to
`app::RemotePlay::start`, which asks that host over `/lan/play`; the shell shows the host's picture
until the session ends. One session at a time either way, and a host plays none while its own game
runs.

- Keys: each side sends 16 fresh random bytes (`salt`); both derive
  `host, guest = HKDF-SHA-256(secret, salt = guest salt || host salt, info = "opensu-stream\n" ("host" | "guest"))`
  (`lan::streamKeys`), one key per sender, new every session.
- Datagrams (`stream::PacketSeal`): a clear 10-byte header (version, kind, the sender's 64-bit
  packet counter), then the payload under ChaCha20-Poly1305 with the sender's key, the counter as
  the nonce and the header as additional data. The opener drops what does not open, and any counter
  already seen or more than 4096 behind the newest (`PacketOpener`): a replayed datagram is never
  taken.
- The host binds a free UDP port for the session (`stream::HostStream`) and takes the first
  datagram that opens as the guest joining; until then it sends nothing, and 20 s without one ends
  it. The guest pings every 200 ms; the host answers with its clock, and the fastest of the last 25
  round trips gives the clocks' offset for the latency stages. Five quiet seconds either way, or
  an End datagram (sent three times), ends the session.
- Host (`app::RemotePlay::offer`, on the listener's thread): refuses outside a Gamescope (the
  screen it would stream), then opens `stream::HostInput` (a `gamepad::RemotePad` on uinput, and
  XTest on that Gamescope's Xwayland for keys and pointer), the stream, `stream::HostVideo`
  (Gamescope's PipeWire node, found as the one owned by the peer of its Wayland socket, NV12
  BT.709 full range; each frame coded with PyroWave on the capture thread, at most 200 Mbit/s at
  60 a second, cut into 1400-byte packets) and `stream::HostAudio` (a PipeWire null sink of the
  session's own, `opensu.remote.<port>`, its monitor coded with Opus in 10 ms frames at 192 kbit/s).
  The loop then starts the game through the normal launch path with `PIPEWIRE_NODE` and
  `PULSE_SINK` naming that sink. The game ending ends the session; the guest leaving closes the
  game.
- Guest: `stream::GuestStream` receives; `stream::GuestVideo` queues video packets and decodes on a
  thread of its own: a frame missing a packet is dropped when the next begins (intra only, nothing
  waits for a resend), and a whole frame two newer ones began after is skipped as late. The loop
  uploads the newest picture and `ui::Shell` draws it letterboxed over black. `stream::GuestAudio`
  decodes each Opus packet, conceals up to ten lost ones, and plays on the shell's device
  (`audio::LiveSound`, at most 60 ms queued) when the shell plays sounds.
- Input: while the remote game has the controls (streaming, no Guide menu or other menu open), the
  shell's pad buttons other than Guide, the first pad's sticks, the held keys (at most 16) and the
  pointer over the picture go to the host (`app::RemoteInput`), each sent when it changes. Guide
  opens this openSU's own menu, whose Close game row reads End remote play.

## Keys and secrets

`lan.json` (config dir) holds this openSU's peer id and each paired peer's id and name. Each
secret is in `secrets::SecretStore` as `lan-peer-<id>`: the desktop keyring when it is there, else
an owner-only file under `<config>/secrets/`. A hidden run uses its own copies and files only.
Secrets are wiped from memory after use (`secrets::wipeBytes`) and never logged.

Remove (Settings › LAN) forgets the peer here at once, and tells it through a signed
`/lan/unpair` when it is announced. After that, its requests are refused and its games leave
Library on both sides.

## Known limits

- Who talks to whom, when, which routes and how much crosses the LAN in clear; what is said does
  not. No forward secrecy: the pairing's secret opens recorded traffic.
- Anyone on the LAN can see that an openSU runs, its name and id, and can ask to pair. The host's
  player sees each request, one at a time.
- A request is good for 120 s of clock skew, and the replay guard forgets nonces after that.
  Machines whose clocks differ by more cannot talk.
- A peer told nothing (it was off the LAN when removed) keeps its half until it is removed there
  too. Its requests are refused here.
- A cover fetched for a local game after the catalog was read is shared from the next catalog
  read.
- Discovery is Linux only. The rest is portable C++ over libcrypto and Lucent.
- A save bundle is held whole in memory on both sides (at most 512 MiB).
- Saves sync only while both openSUs run. One that was off syncs at its next launch of the game,
  or when its details open and Sync saves now is pressed.
- A game whose emulator keeps no saves openSU can find (store games, standalone emulators of
  consoles other than the Switch) shares none.
- Remote play is Linux only both ways (PipeWire, uinput, XTest, PyroWave built for Linux), and only
  with PyroWave: no fallback encoder or bandwidth probing yet (step 3 of
  `docs/plans/lan-remote-play.md`). Wired gigabit is the intended link.
- The guest decodes PyroWave into CPU memory and converts to RGBA there, and uploads a new texture
  each frame; decode p50 is 5–10 ms on a Radeon RX 6700 XT shared with other work.
- Remote triggers are digital (full travel or none), right and middle mouse buttons are held for
  one frame, and the remote pad gets no per-game mapping. The guest's Video settings do not apply
  to the stream.
- A game whose sound ignores `PIPEWIRE_NODE` and `PULSE_SINK` plays on the host's speakers and
  the guest hears silence.

## Checking it

`tests/lan/lan_node_test.cpp` runs two nodes and a stranger in one process on loopback, with a
stand-in directory in place of Avahi, and two more sharing a runner game's saves: pulled before a
launch, pushed after, a conflict left alone until picked, a pick refused while the emulator runs,
the replaced saves backed up. In the product, start two or three `--hidden` openSUs, each with its
own `HOME` and the XDG dirs under it and `"integrations": {"lan": true}` in its settings. Drive
Settings › LAN through each control channel. `/state` reports `lanSeen`, `lanPaired`, `lanCode`
and, with a details page open, `lanSaves` (its Saves fact); `/frame.png` shows the code on both
screens, and the `lan` and `saves` log lines name every pairing, listing, refusal, pull and push.
A request to `/lan/library` without a pairing gets 403. A socket dump of the LAN port shows the
routes and lengths but no save bytes.

Remote play: `tests/stream/stream_test.cpp` checks sealing and replay, the payloads, a host and a
guest session over loopback UDP, PyroWave round trips and a lost packet, and Opus with a lost
packet concealed. In the product, run the host as the only client of a headless Gamescope
(`gamescope --backend headless -W 1280 -H 720 -r 60 -- opensu --hidden`, `WAYLAND_DISPLAY` set to
empty, the user's `XDG_RUNTIME_DIR` for PipeWire) and the guest as a plain `--hidden` openSU, pair
them and press Play on the host's game in the guest's Library. `/state` reports `remotePlay`
(`asking`, `playing`, `hosting`), `remotePeer`, `remoteRoundTripUs` and `remoteBytesPerSecond`,
and on the guest `remoteVideo` (frames decoded, dropped and late, and each stage's p50/p95 in ms
from capture to the submitted frame) and `remoteAudio` (packets, lost, arrival p50/p95). On both
sides `/remote/frame.png` returns the newest remote picture, numbered in `X-Opensu-Frame`: the one
sent on the host and the one decoded on the guest, so the same number can be compared. A hidden
guest has no surface: `submitted` counts frames submitted and `drawn` stays 0.
