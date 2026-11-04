# A quit waits for the artwork download under way

Status: open.

Observed 2026-11-04 in hidden runs on a fresh demo home with its ROMs (the cover fit already
abandoned on quit, `quit-waits-for-a-cover-fit.md`): `POST /quit` took 0.8 to 1.0 s in three of six
rounds and 7 s in one, while libretro-thumbnails' box art listings were downloading. A stack taken
0.25 s after the quit: the main thread in `ShellApp::~ShellApp` →
`artwork::ArtworkFetcher::~ArtworkFetcher` joining its worker.

Cause, by reading: `src/artwork/artwork_fetcher.cpp` `ArtworkFetcher::run` looks at its stop token
only between items, and `fetch` (`keep`, `index` → `boxArtNames`, `fetchArcadeNames`, `fetchGog`)
downloads through `net::WebClient::get` without one.

Fix to make: pass the worker's stop token to those downloads (`WebClient::get`'s `stop`, as the
cover fitter does) and end the item without a miss or a dropped queue once it is requested.
