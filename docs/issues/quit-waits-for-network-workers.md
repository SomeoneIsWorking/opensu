# A quit waits for a worker's network call that ignores its stop

Status: open.

Found 2026-11-05 by reading, auditing every `net::WebClient` call after the artwork fetcher and
giveaway feed fixes (`quit-waits-for-an-artwork-download.md`, `quit-waits-for-the-giveaway-feed.md`).
Not reproduced yet. `ShellApp`'s members are destroyed one after another, each joining its worker,
so each worker below that is mid-request holds the quit up until libcurl gives up (10 s to connect,
30 s for the transfer, per call), and the waits add up. A `download` stops within about a second
through its progress callback and is not listed.

Starts on its own:

- `library/catalog_loader.cpp` `CatalogLoader::run` → `gog.cpp` `Provider::list`,
  `gog_builds.cpp` `offeredBuild`, `gog_auth.cpp` `Auth::request`: `Provider::list()` takes no stop.
- `lan/lan_node.cpp` `LanNode::work` → `fetch`, `coverOf`, `tell` → `peer_client.cpp`
  `PeerClient::send`: the token is in `work`; `PeerClient` never sets `Guard::stop`.
- `plugin/host_api.cpp` `httpGet`, on `plugin/io_pool.cpp` `IoPool`: a job is a
  `std::function<void()>`, so the pool's token never reaches it.
- `app/wine_runners.cpp` `runCheck` → `updateGeProton`, `updateUmu` (`emulator_release.cpp`
  `EmulatorRelease::latest`), the checksum `get` and `updateDatabase`: the token is in scope.
  Hidden runs never check, so it cannot be shown hidden.
- `library/romset_lists.cpp` `RomsetLists::fetch` → `romset_dats.cpp` `fetchRelease` →
  `EmulatorRelease::latest`.
- `save_sync/save_sync.cpp` `SaveSync::run` → `lan/peer_save_target.cpp`,
  `save_sync/webdav_share.cpp` `send`: `SaveTarget` takes no stop; it runs after every game exit.

Started by the player: `lan/guest_pairing.cpp` `pair`, `lan/rom_import.cpp` `fetchFile`,
`app/emulator_downloads.cpp` (`runCheck`: `WebClient::head` has no stop, `EmulatorRelease::latest`,
`libretro_cores.cpp` `index` and `infoArchive`; `installRelease`, `installCore`),
`artwork/cover_offers.cpp` `CoverOffers::perform`, `app/game_match_controller.cpp`
(`findSteamApps`, `cached`, `findBoxArt` → `boxArtNames` without its `stop`),
`app/own_games_controller.cpp` `identify` (no token), `lan/lan_service.cpp`'s askers (no token),
`app/gog_install_job.cpp` `arguments` and `app/sign_in.cpp` `complete` → `Auth::request`.

Fix to make: every worker joined at quit passes its stop token to every `WebClient` call, as
`ArtworkFetcher` and `readStores` do, and ends the item keeping nothing once it is requested.
`WebClient::head`, `Provider::list`, `SaveTarget` and `IoPool`'s jobs take a stop first, each its own
restructuring commit.
