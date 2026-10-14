# Game updates per store

Status: open.

Requested: "game updating".

## Current state

- **Steam** updates itself, and openSU follows it.
  - `steam/client:Client::installed` is false while the manifest says an update is pending.
  - `steam/downloads` lists updates in Steam's queue (`Download::installing` false).
  - A launch of a game with an unfinished download waits with `Updating · N%` (project-state S016,
    `launch/handoff.cpp`).
  - There is no "update available" mark and no way to start an update Steam has not scheduled.
- **Epic:** `library/epic:Provider::list` runs `legendary list --json` and
  `legendary list-installed --json`, but reads neither version. legendary 0.20.35 can update with
  `legendary update <app>` (an alias of `install`, same progress lines) and reports the available
  version only through `list-installed --check-updates --csv/--tsv`. The `--json` output carries the
  installed `version` only (`legendary/cli.py:list_installed`). The asset's `build_version` is in
  `list --json`'s `asset_infos`.
- **GOG:**
  - `app/gog_install_job` runs `gogdl download`. gogdl 1.3.1 has `update` and `repair` as aliases
    of `download`.
  - `library/gog_installs:InstallRecords` records path and platform, but no build.
  - `gogdl import <path>` prints the installed `buildId` (`gogdl/imports.py:get_info`).
  - `Game::builds` holds platforms, not build ids.
  - project-state S007: "No uninstall, update or repair yet."
- **Emulators and cores** already have a check and an update (`app/emulator_downloads`,
  S026/S031). They are listed here only because the downloads page shows them.

## iiSU reference

iiSU's Settings › Updates (`l64.E/F`, "UpdateRepository" in `qs8`/`yr8`) updates only iiSU itself
(the APK, Stable and Nightly supporter channels) and its data files (`emuladores.json`,
`emulator_options.json`, the starter pack). The notification actions are `sm4` DownloadApkUpdate,
InstallApkUpdate and ApplyArtifactUpdate. iiSU does not update games or emulators, so the game side
is openSU's own. An openSU self-update (the AppImage already carries zsync update information,
`tools/appimage/pack`) would be the counterpart of iiSU's section but was not requested.

## Gap

- No per-game "update available" state from Epic or GOG.
- No update action for Epic or GOG.
- No "update now" for a Steam game whose update is not queued.
- No place that lists the available updates.

## Proposed owners and design

1. `library/game.hpp`: add `Game::updateAvailable`, set by the providers and shown as a dot on the
   tile and a line on the details page.
2. Epic (`library/epic`): parse `version` from `list-installed --json` and `asset_infos.<platform>
   .build_version` from `list --json` (both already run). An update is available when they differ.
   No extra process is needed. The update itself is `EpicInstallJob` with a verb, `legendary update
   <app> -y --skip-sdl --base-path ...`, with the same progress parse (`library/install_log`).
3. GOG:
   - `InstallRecords` gains `build` (gogdl's `buildId`), recorded after a download through `gogdl
     import <path>`. Existing records get it on the next listing.
   - `library/gog` reads the latest build from the content system (`content-system.gog.com/products
     /<id>/os/<platform>/builds?generation=2`, the host gogdl uses, `gogdl/constants.py`) for
     installed games only.
   - The update is `GogInstallJob` running `gogdl update <id> --platform <p> --path <recorded
     root>`. It refreshes the token the same way as an install.
4. Steam: `steam/download_control` (from `downloads-page.md`) gains `queueUpdate(appId)` through
   `SteamClient.Downloads.QueueAppUpdate(appId, clientId)`, as Steam's library page calls it
   (`chunk~2dcc5aaf7.js`). Steam's per-app auto-update policy (`SteamClient.Apps
   .SetAppAutoUpdateBehavior`) is left to Steam.
5. `app/installs`: `start(game, Verb::Install | Verb::Update)`. The update shares the one job and
   the queue. The details page shows "Update" when `updateAvailable` (`ui/details_page`,
   `DetailsAction::Update`). The downloads page lists "Updates available" with "Update all".
6. `launch/handoff`: launching an Epic or GOG game with an update available launches the installed
   version. Asking first is an open question.

## Tests

- `epic` parse of both versions from fixtures.
- `gog_installs` with a build.
- `gog` latest-build parse against `tests/support/release_server.hpp`.
- `epic_install_job` and `gog_install_job` update argv against stubs.
- `download_control.queueUpdate` against `fake_steam`.
- Details page Update button.

Live verification needs a signed-in Epic or GOG account with an outdated game. Neither store is
signed in on this machine (project-state, "Installing Steam games").

## Depends on

`downloads-page.md` (install job status and queue, `steam/download_control`).

## Open questions

- Should updates check when the catalog loads (proposed, free for Epic, one request per installed
  game for GOG) or only on request?
- Should openSU ever update Epic or GOG games on its own, or only when the player asks (as
  emulators do now)?
- Should a launch with an update pending ask to update first?
