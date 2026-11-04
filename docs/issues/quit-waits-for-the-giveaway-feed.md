# A quit waits for the giveaway feed's store read

Status: open.

Observed 2026-11-05 in the artwork download reproduction
(`quit-waits-for-an-artwork-download.md`) with every host's CONNECT held by the proxy: `POST /quit`
took 9 s with the main thread in `ShellApp::~ShellApp` → `giveaways::GiveawayBoard::~GiveawayBoard`
joining its worker, which was in `offer_feed.cpp` `readEpic` → `fetch` → `WebClient::get`.

Cause, by reading: `src/giveaways/offer_feed.cpp` `readStores` looks at its `stop` only between
stores, and `readEpic`, `readGog` and `readSteam` download through `fetch` without it.

Fix to make: pass `readStores`' `stop` to `WebClient::get` in `fetch`, and end the read without a
store error once it is requested.
