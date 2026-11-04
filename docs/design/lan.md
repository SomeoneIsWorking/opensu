# LAN

The LAN integration (Settings › Integrations › LAN, off by default) makes this openSU findable by
the other openSUs on the network, pairs with them, lists the installed games of each paired one in
Library under that openSU's name, shares the games' saves with the ones the player chose, and
plays such a game remotely: it runs on the host and streams here (see [Remote play](#remote-play)),
and imports a paired openSU's ROMs into this one's ROM folder (see [ROM import](#rom-import)).
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
- The node adds the peers the player shares saves with to `save_sync::SaveSync` (owned by the
  shell's `app::SaveSharing`) as `PeerSaveTarget`s; see [Shared saves](#shared-saves).
- `app::RemotePlay` (on the shell's loop, both ways) runs remote play's sessions; see
  [Remote play](#remote-play).
- `lan::RomShelf` (in the node) serves the catalog's ROM files and BIOS files to paired peers, and
  `lan::RomImport` (in the node) fetches the ones the player picked from a host; `app::LanRoms`
  (on the shell's loop) gives both to the shell. See [ROM import](#rom-import).

## Discovery

Each node announces `_opensu._tcp` on its listener's port with TXT `id` (its 32-hex peer id),
`name` (at most 63 bytes, no control bytes) and `v` (protocol `2`: sealed bodies). On Linux this goes through
Avahi's D-Bus API (`EntryGroupNew`/`AddService`/`Commit`, `ServiceBrowserNew`, `ResolveService`)
over sd-bus on the node's own thread. A name clash takes Avahi's alternative name. Only IPv4
resolutions are kept, and its own id is skipped. An openSU on the same machine resolves on every
interface, loopback included; the first one is listed.

On Android, `dns_sd_android.cpp` calls the Activity over JNI, and `LanDiscovery.java` registers
and browses through `NsdManager`, resolving one service at a time (NSD allows one resolve at a
time before API 34) and keeping IPv4 addresses only. It holds a `WifiManager` multicast lock while
the node runs (`CHANGE_WIFI_MULTICAST_STATE`; many devices drop multicast without it); NSD needs no
runtime permission, and `NEARBY_WIFI_DEVICES` is not asked for. The machine's name is the
device name from Settings, else the model. Windows and macOS build `dns_sd_none.cpp`, and
Settings › LAN says finding openSUs is not built there yet.

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
| `GET /lan/roms` | paired, signed | the ROM listing (below), at most 8 MiB |
| `POST /lan/roms/file` | paired, signed | `{"kind","root","path"}` → `{"bytes","sha256"}`; `sha256` is empty while the host still hashes the file |
| `POST /lan/roms/chunk` | paired, signed | `{"kind","root","path","offset","length"}` → at most 2 MiB of the file from `offset`, as raw bytes |
| `POST /lan/play` | paired, signed | `{"game","salt"}` → `{"port","salt"}`: the host streams that listed game to the caller; 404 for a game it does not list, 409 with why when it will not (busy, outside Gamescope, no capture) |

Everything else, and any signed route that fails its check or whose body does not open, is 403.
The saves routes are 403 too, sealed, while this openSU does not share saves with the caller. The
ROM routes are 403 for any path that is not a file the host lists (below).

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
- The record (`save-sync.json`, state dir, shared with the folder and WebDAV targets): per game a change counter, raised whenever the content
  hash here changes, the hash (SHA-256 over each file's slot kind, path and bytes; times do not
  count), and per target (a peer's id) the hash both held when they last synced.
- The rule (`syncStateOf`): the same hash is in sync; no save on one side takes the other's; else
  a side whose hash differs from the last-synced one changed. One side changed: its saves go to
  the other. Both changed, or two openSUs that never synced hold different saves: conflict, and
  nothing is replaced until the player picks.
- When: before a game starts (`SaveSharing::before` holds the launch on the shell's loop, "Syncing
  X's saves first", until `SaveSync::prepared`); after it ends; from Sync saves now; on opening its
  details (look only); when a peer says it has newer saves (`/news`) or took these (`/synced`).
- A pull asks `/status`, takes the bundle from `/get`, checks its hash against the status, stages
  it under `<state>/saves-incoming/<hash of the target>/`, checks that the saves here did not change meanwhile and
  are not in use (the game the shell plays, or the emulator's process through
  `launch::runningRefusal`), and copies it in, the saves replaced backed up first under
  `<state>/save-backups/`. It then records the hash and tells the peer (`/synced`).
- A push is only `/news`: the peer pulls, under its own checks. Request bodies stay small.
- In a conflict the details row is Pick which save to keep: a dialog with each side's date and
  size and three buttons: Cancel, Keep this one (focused; tells the peer with `picked` set, and it
  pulls over its own) and Keep X's (pulls).
- The engine is `save_sync::SaveSync`, the same for every target ([cloud saves](../plans/cloud-saves.md)):
  the LAN is one group of targets (`PeerSaveTarget`), listed afresh each sync from the paired
  peers that share saves, and dropped when the node stops.
- The work runs on `SaveSync`'s thread, one game at a time; no lock is held while a peer is asked,
  so two openSUs syncing each other cannot deadlock. A hidden run keeps the record, staging and
  backups in its own folder (`PlayerFiles::stateDir`).

The Saves fact on the details page shows the peer most worth attention: Conflict with X, Newer on
X, the reason a sync failed, Newer here than on X, X does not share saves with this openSU, X cannot
be reached, In sync with X.

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
  BT.709 full range; each frame coded on the capture thread with the link's codec and rate, cut
  into 1400-byte packets, each carrying its codec, key flag and part of parts) and `stream::HostAudio` (a PipeWire null sink of the
  session's own, `opensu.remote.<port>`, its monitor coded with Opus in 10 ms frames at 192 kbit/s).
  The loop then starts the game through the normal launch path with `PIPEWIRE_NODE` and
  `PULSE_SINK` naming that sink. The game ending ends the session; the guest leaving closes the
  game.
- Codec and link (`stream::LinkRate`, owned by `HostStream`): the host offers what it codes
  (`encodableHere`: PyroWave where built, and each of AV1, HEVC and H.264 its VA-API driver
  encodes) and each guest Report says what it decodes. Once the guest joins, a prober thread sends
  a 300 ms burst of 1428-byte Probe datagrams at 220 Mbit/s; the guest counts what came and
  reports with each ping (datagrams and sealed bytes received, the newest counter, the probe burst
  heard). PyroWave codes while 80 % of what the link delivered is at least 100 Mbit/s (its budget
  that share, at most 200 Mbit/s); below that the best hardware codec both ends have codes at 60 %
  of it, 2 to 40 Mbit/s. While running, each second of reports is judged: over 5 % lost (1 % on a
  hardware codec, which decodes nothing from a loss to the next key frame) replans at what was
  delivered; under 1 % (0.1 % on hardware) raises the rate (×1.25 PyroWave, ×1.15 hardware). On a
  hardware codec a 200 ms probe at 131 Mbit/s every 10 s (doubling to 60 s each time one loses
  more than 2 %) brings PyroWave back once one passes. A codec that will not open on the host is
  refused and the next planned. Every switch codes a key frame. With neither end's hardware codec
  in common PyroWave codes whatever the link carries, and says so.
- Hardware codecs (`hardware_codec_vaapi.cpp`): libavcodec on a VA-API DRM device, CBR at the
  plan's rate with a two-frame buffer, no B-frames, one frame in flight, a key frame every 300
  frames or when asked, reopened when the rate moves by a fifth.
- Pacing (`HostStream::sendFrame`, a `stream::TokenBucket`): each frame's datagrams leave at once
  while the bucket holds them, the bucket as deep as one frame at the plan's rate (headers and
  seals counted) and filling at twice that rate; the rest of a larger frame, a key frame above
  all, follows from the sender thread as the bucket fills, and the next frame waits behind it. A
  frame no larger than the plan's (every PyroWave frame) leaves as before; a shallow link queue
  never gets more than one frame's worth at line rate. The guest decodes the same way
  into NV12 and converts it to RGBA on the CPU.
- Guest: `stream::GuestStream` receives; `stream::GuestVideo` queues video packets and decodes on a
  thread of its own, a decoder per codec: a frame missing a packet is dropped when the next begins
  (nothing waits for a resend); on a hardware codec every frame after it is dropped until a key
  frame, which the guest asks for while the decoder waits (KeyFrame, at most every 100 ms); a whole PyroWave frame two
  newer ones began after is skipped as late. The loop
  uploads the newest picture and `ui::Shell` draws it letterboxed over black. `stream::GuestAudio`
  decodes each Opus packet, conceals up to ten lost ones, and plays on the shell's device
  (`audio::LiveSound`, at most 60 ms queued) when the shell plays sounds.
- Input: while the remote game has the controls (streaming, no Guide menu or other menu open), the
  shell's pad buttons other than Guide, the first pad's sticks and triggers' travel (0–255), the
  held keys (at most 16) and the pointer over the picture with its buttons held and the wheel's
  whole notches go to the host (`app::RemoteInput`, sampled once a frame, a press and release in
  one frame sent held for it), each sent when it changes. The guest's mapping for the game
  (`PadOwner::forwarding`, the same `ForwardMap` a local launch gives its pads, and the chord hold
  delay) goes with each ping; the host's `RemotePad` plays the pad through a `PadTranslator` with
  it, as a local game's virtual pad is played. Guide
  opens this openSU's own menu, whose Close game row reads End remote play.

## ROM import

The host offers what its catalog reads from its ROM folders, and the BIOS files its runner has for
those consoles. The guest browses them in Settings › LAN › ROMs on <host>, picks games or a whole
console, and Import copies them into its own ROM folder, where the catalog reads them. Library's
Import from a PC on your network tile, shown while LAN is on, opens Settings › LAN with
`LanRoms::entry` over it: the one paired host's page, Import ROMs from (each paired host) when
there are several, nothing more when none is paired (`SettingsController::openLanImport`).

- **Host.** `app::LanRoms::share`, called from `ShellApp::applyCatalog`, gives the node's
  `lan::RomShelf` the ROM roots, the ROM games and, for each of their consoles, the BIOS files the
  chosen built-in core lists and the system folder holds (`EmulatorSettings::biosOf`). A game is
  its catalog entry under a root: one file, or a folder (a disc's `.cue` and its tracks, a
  multi-file game) walked whole. The listing is
  `[{"system","folder","games":[{"title","root","entry","files":[{"path","bytes"}]}],"bios":[...]}]`,
  consoles grouped by system and the host's folder for it. Paths are relative, `/`-separated and
  checked by `fileio::staysInside` (no absolute path, drive, `.` or `..` step), the rule
  `save_bundle` checks slots with. `RomShelf` answers a file or a chunk only for a key
  (kind, root, path) it listed, so a path outside the listed folders, or a file in them it does
  not list (`notes.txt`), is 403. It reads only the asked range; nothing holds a whole ROM.
  Hashing runs on its own thread, one file at a time, kept by size and modification time; the
  first `/lan/roms/file` of a file queues it.
- **Guest.** `lan::RomImport` (its own thread) fetches each picked game file by file in 2 MiB
  chunks (`wire::romChunkBytes`), each a signed, sealed request, appending to
  `<state>/lan-roms/<host id>/{rom-<root>|bios}/<path>.part`. A part already there is hashed and
  the transfer goes on from its end ("goes on from byte N"). When the part reaches the host's size
  it waits for the host's SHA-256 and compares; a mismatch deletes the part and fails the game
  ("arrived different from <host>'s copy"). A whole file is placed through
  `library::roms::RomTree::place` into the destination folder, then the next. A network error, or
  the host off the LAN, leaves the game Waiting and tries again after 1 s, doubling to 30 s; a
  refusal or a failed place fails it. Stop importing drops what has not arrived and its parts.
- **Destination.** The first ROM folder the catalog reads that `RomTree::writeRefusal` accepts;
  with none, the guest asks for one (the folder chooser on the desktop, the system picker on
  Android, which grants write access to the tree), and it joins the ROM folders
  (`Settings::addRomFolder`, applied at once by `ShellApp::romFoldersChanged`). A console's games
  go in the folder the guest already has for that system, else one named as the host's.
  BIOS files go to the runner's system folder.
- **BIOS.** For each picked console the host's BIOS files the guest's core lists and the guest's
  system folder lacks are offered as toggles; on when the core cannot start without one and the
  file is not optional, off otherwise.
- **Progress.** Each game's row shows Queued, its percentage, Waiting with the reason, Checking,
  Done or Failed with why; All shows the total. `/state` reports `lanImport` ("title state
  done/total" per game). Each arrival reloads the catalog.
- **Android.** `RomTree::place` into a picked folder (`content://`) goes through
  `DocumentTree.place`: it makes the subfolders with `DocumentsContract.createDocument`, replaces a
  document of the same name and copies the staged file through `openOutputStream`, then deletes
  the staging copy.

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
- Discovery is Linux and Android only. The rest is portable C++ over libcrypto and Lucent.
- An Android host does not offer ROMs from a picked (`content://`) folder; only folders on a path.
- A ROM whose path has a `:` or `\` is not offered (the relative-path rule).
- The import queue is not kept across restarts; the parts are, so importing the same game again
  goes on from them.
- A save bundle is held whole in memory on both sides (at most 512 MiB).
- Saves sync only while both openSUs run. One that was off syncs at its next launch of the game,
  or when its details open and Sync saves now is pressed.
- A game whose emulator keeps no saves openSU can find (store games, standalone emulators of
  consoles other than the Switch) shares none.
- Remote play is Linux only both ways (PipeWire, uinput, XTest, PyroWave and VA-API built for
  Linux); Windows, macOS and Android guests are not built.
- Hardware codecs need a VA-API driver that encodes (host) or decodes (guest) them. Fedora's own
  Mesa encodes none and decodes only AV1 there; RPM Fusion's `mesa-va-drivers-freeworld` adds
  H.264 and HEVC. The AppImage is built without them (`OPENSU_BUILD_VAAPI=OFF`: libavcodec's
  soname differs per distro), so it streams PyroWave only.
- The guest decodes into CPU memory, converts to RGBA there and uploads a new texture each frame.
  Decoding straight into an image the presenter samples needs the decoder on the presenter's
  Vulkan device, which SDL GPU neither exposes nor imports (`SDL_GPUDevice` gives no `VkDevice`,
  queue or external-memory import); it waits on an SDL fork that does.
- The guest's Video settings do not apply to the stream.
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

ROM import: `romsComeOverInChunks` in `lan_node_test.cpp` lists a host's ROMs and BIOS, imports a
game larger than two chunks that resumes from a part, fails a damaged part on its hash, and waits
while the host is away; `signedRequestsAreCheckedOnce` asks for unlisted and `..` paths and gets
403. In the product, give the host a scratch ROM folder through `OPENSU_ROM_ROOTS`, pair, and on
the guest walk Settings › LAN › ROMs on <host>, a console, Whole console, Back, Import; `lanImport`
reaches `done` for each game, the guest's library shows them and they play. Kill the host by its
PID during a large file and start it again: the guest's log says "goes on from byte". A signed,
sealed `/lan/roms/chunk` for `../x` or an unlisted file gets 403 and the host logs the refusal.

Remote play: `tests/stream/stream_test.cpp` checks sealing and replay, the payloads, a host and a
guest session over loopback UDP (the mapping sent, the opening probe planning PyroWave), PyroWave
round trips and a lost packet, each hardware codec this machine codes and decodes over moving
pictures with a lost packet held until the next key frame, `LinkRate`'s choices (a whole probe,
a third of one, no hardware codec in common, a lossy second, a probe passing, a refused codec),
`LinkCap` carrying its rate, and Opus with a lost packet concealed. In the product, run the host as the only client of a headless Gamescope
(`gamescope --backend headless -W 1280 -H 720 -r 60 -- opensu --hidden`, `WAYLAND_DISPLAY` set to
empty, the user's `XDG_RUNTIME_DIR` for PipeWire) and the guest as a plain `--hidden` openSU, pair
them and press Play on the host's game in the guest's Library. `/state` reports `remotePlay`
(`asking`, `playing`, `hosting`), `remotePeer`, `remoteRoundTripUs` and `remoteBytesPerSecond`;
on the host `remoteVideo` is the link (codec, rate, delivered, loss, switches, probes, and `paced=` the video datagrams the pacer held back of those sent), on the guest
the codec and switches, frames decoded, dropped and late, and each stage's p50/p95 in ms from
capture to the submitted frame and `remoteAudio` (packets, lost, arrival p50/p95). On both
sides `/remote/frame.png` returns the newest remote picture, numbered in `X-Opensu-Frame`: the one
sent on the host and the one decoded on the guest, so the same number can be compared. A hidden
guest has no surface: `submitted` counts frames submitted and `drawn` stays 0. `POST /remote/link`
with bits a second caps what a hidden guest takes (`stream::LinkCap`, a token bucket 20 ms deep;
0 lifts it), the slow link that makes the host fall back; the `stream` log lines name each probe
and plan. `POST /pointer` `down X Y right` and `up X Y right` hold a button across frames.
