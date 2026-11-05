# A quit waits for a worker's network call that ignores its stop

Status: closed.

Found 2026-11-05 by reading, auditing every `net::WebClient` call after the artwork fetcher and
giveaway feed fixes (`quit-waits-for-an-artwork-download.md`, `quit-waits-for-the-giveaway-feed.md`).
`ShellApp`'s members are destroyed one after another, each joining its worker, so a worker
mid-request held the quit until libcurl gave up (10 s to connect, 30 s for the transfer, per call).

Cause: `net::WebClient`'s requests took the stop as an option (`Guard::stop`, a defaulted `get`
argument, none on `head`), so each caller had to remember it; `Provider::list`, `SaveTarget`,
`SaveShare`, `IoPool`'s jobs and `PeerClient` had no way to pass one.

Fix (`docs/design/net.md`): every `WebClient` request takes a `std::stop_token` first, with no
default, and a stopped one comes back `net::Stopped`, not a failure. The token is threaded through
`Provider::list`, `SaveTarget`/`SaveShare`, `IoPool` jobs and `PeerClient`, and each worker passes
its own and keeps and logs nothing of a stopped request: the catalog loader (GOG), the plugin I/O
pool, the romset listings, the Wine runner check, save sync, the LAN node, pairing, ROM import and
the play and ROM askers, the emulator update check, Choose artwork, the match search, the added
game's lookup and the GOG install's token refresh. The GOG sign-in's code exchange keeps an explicit
`std::stop_token{}`: it runs on the control channel's thread, which has no stop.

Shown 2026-11-05, hidden, every request through a proxy that holds the worker's host
(`scratch/netstop/proof/quit_proof.py`), quit to exit over three timed rounds, the held thread named
by gdb in a fourth: GOG listing (`CatalogLoader::run` → `gog::Provider::list`) 0.032, 0.032,
0.016 s; plugin `http.get` (`IoPool`) 0.032 s ×3; romset listing (`RomsetLists::run` →
`RomsetDats::refresh`) 0.032 s ×3; LAN polling of an announced paired peer that never answers
(`LanNode::work` → `fetch`) 0.032 s ×3; save sync to WebDAV after a game exit
(`SaveSync::run` → `ShareTarget::unlock`) 0.032 s ×3. None logged a warning after the quit.
The Wine runner check never runs hidden; `tests/app/wine_runners_test.cpp` holds its request on a
slow loopback server and shows `reset()` ending it within 1 s, and fails without the token. The
player-started workers (emulator update check, Choose artwork, match search, added-game lookup, LAN
pairing, ROM import and play, GOG install) were not timed in the product.
