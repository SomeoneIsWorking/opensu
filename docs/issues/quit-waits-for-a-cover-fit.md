# A quit waits for the SteamGridDB cover being fitted

Status: fixed.

Observed 2026-11-04: in a hidden run on the player's library while SteamGridDB covers are being
fitted, `POST /quit` and SIGTERM take 1 to 3.5 s to exit, against 0.03 to 0.06 s with
`OPENSU_STEAMGRIDDB_API` pointed at a closed port. gdb during the wait: the main thread in
`ShellApp::~ShellApp` → `SteamGridDbCovers::~SteamGridDbCovers` →
`artwork::CoverFitter::~CoverFitter` joining its worker.

Reproduced on 447c326 with the API stand-in answering after 4 s
(`steamgriddb_api_stand_in --slow 4000`), a fresh demo home per round (`tools/screenshots/demo.py`
Steam games, no ROMs), a key in `OPENSU_SECRET_STORE=file` and `POST /quit` at varied moments once
the library loaded: 8 rounds took 2.0 to 7.6 s to exit.

Cause: `src/artwork/cover_fitter.cpp` `CoverFitter::fit` made its SteamGridDB lookups and the
image download through `net::WebClient` calls nothing could interrupt, and `CoverFitter::run`
looked at its stop token only between games, so `~CoverFitter`'s join waited for the listing and
the image of the game under way (up to two 4 s answers).

Fix: `WebClient::Guard::stop` and `WebClient::get`'s `stop` end a transfer as soon as the stop is
requested: the transfer runs on a curl multi handle that a `std::stop_callback` wakes, so it ends
whether it is connecting or waiting for the answer. `SteamGridDb::covers` passes its `stop` to every
API call, and `CoverFitter::fit` passes the worker's stop token to the lookup and the image; once
it is requested the fit ends with nothing kept, no miss recorded and nothing logged. A cover only
ever reaches the cache whole (`fileio::writeWhole`, after the whole image arrived).

Shown 2026-11-04, the same runs on the fix: 8 rounds exited in 0.03 to 0.11 s; after each round
every file in `<cache>/artwork/fitted/` was a whole PNG and no `.part` was left.

The artwork fetcher has the same wait for its own downloads:
`docs/issues/quit-waits-for-an-artwork-download.md`.
