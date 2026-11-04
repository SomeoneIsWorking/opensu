# A quit waits for the artwork download under way

Status: fixed.

Observed 2026-11-04 in hidden runs on a fresh demo home with its ROMs (the cover fit already
abandoned on quit, `quit-waits-for-a-cover-fit.md`): `POST /quit` took 0.8 to 1.0 s in three of six
rounds and 7 s in one, while libretro-thumbnails' box art listings were downloading. A stack taken
0.25 s after the quit: the main thread in `ShellApp::~ShellApp` →
`artwork::ArtworkFetcher::~ArtworkFetcher` joining its worker.

Reproduced on f877d42: a fresh home with two ROMs (NES, SNES) and only the emulators integration,
run hidden in a network namespace of its own with `https_proxy` pointed at a loopback stand-in that
holds each CONNECT to libretro-thumbnails unanswered for 20 s and refuses every other host;
`POST /quit` once the box art listing's CONNECT arrived took 8.6 to 9.2 s in four rounds (libcurl's
10 s connect limit). gdb during the wait: the main thread in `~ArtworkFetcher` joining the worker,
which was in `ArtworkFetcher::index` → `boxArtNames` → `WebClient::get`.
(`OPENSU_STEAMGRIDDB_API` reaches only the cover fitter, not this fetcher, hence the proxy.)

Cause: `src/artwork/artwork_fetcher.cpp` `ArtworkFetcher::run` looked at its stop token only
between items, and `fetch` (`keep`, `index` → `boxArtNames`, `fetchArcadeNames`, `fetchGog`)
downloaded through `net::WebClient::get` without one, so `~ArtworkFetcher`'s join waited for the
transfer under way.

Fix: `ArtworkFetcher::run` passes the worker's stop token to every download, through `boxArtNames`
and `fetchArcadeNames` too; once it is requested the item ends as `Outcome::Stopped` and the worker
returns with nothing kept, no miss recorded, the queue left alone and nothing logged. Artwork and
listings only reach the cache whole (`fileio::writeWhole`, after the whole body arrived).

Shown 2026-11-05, the same runs on the fix: 8 rounds exited in 0.016 to 0.035 s, and no file
(artwork, listing or miss marker) was left under `<cache>/opensu`.

The giveaway feed has the same wait for its store reads:
`docs/issues/quit-waits-for-the-giveaway-feed.md`.
