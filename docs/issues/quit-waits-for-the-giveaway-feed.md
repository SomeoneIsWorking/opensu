# A quit waits for the giveaway feed's store read

Status: fixed.

Observed 2026-11-05 in the artwork download reproduction
(`quit-waits-for-an-artwork-download.md`) with every host's CONNECT held by the proxy: `POST /quit`
took 9 s with the main thread in `ShellApp::~ShellApp` → `giveaways::GiveawayBoard::~GiveawayBoard`
joining its worker, which was in `offer_feed.cpp` `readEpic` → `fetch` → `WebClient::get`.

Reproduced on 662a6bf: a fresh home with two ROMs and only the emulators integration, run hidden in
a network namespace of its own with `https_proxy` pointed at a loopback stand-in that holds each
CONNECT to Epic's promotions host unanswered for 20 s and refuses every other host; `POST /quit`
once that CONNECT arrived took 8.5 to 9.1 s in three rounds (libcurl's 10 s connect limit). gdb
during the wait: the main thread in `~GiveawayBoard` joining the worker, which was in
`GiveawayBoard::read` → `readStores` → `readEpic` → `WebClient::fetch`.

Cause: `src/giveaways/offer_feed.cpp` `readStores` looked at its `stop` only between stores, and
`readEpic`, `readGog` and `readSteam` downloaded through `fetch` without it, so
`~GiveawayBoard`'s join waited for the transfer under way.

Fix: `readStores` passes its `stop` through every reader to `WebClient::get`; once it is requested
the read ends with neither the store under way nor the rest, nothing logged, and
`GiveawayBoard::read` keeps nothing of it (no offers, no failure, no `offers.json`).

Shown 2026-11-05, the same runs on the fix: 8 rounds exited in 0.016 to 0.032 s, nothing was logged
for the giveaways and no file was left under `<cache>/opensu`.

The other workers that make a quit wait on a network call the same way:
`docs/issues/quit-waits-for-network-workers.md`.
