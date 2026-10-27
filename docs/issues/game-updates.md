# Game updates per store

Status: partial. Built and verified by unit tests through the shipping owners and the
`update-badge` and `details-update` scenes of the store run against a stand-in `legendary`; no real
Steam, Epic or GOG update has been run.

Requested: "game updating".

## Decisions

- Updates are checked when the catalog loads; there is no timer of their own.
- openSU never updates a game by itself; the player presses Update.
- A launch does not ask to update first; it starts the installed version. The tile and the details
  page show that an update is there.

## Behaviour

- An installed Steam, Epic or GOG game whose store has a newer build has `Game::updateAvailable`:
  - its tile carries a down arrow on the store icons' dark disc in the content's top-right corner
    (`ui::updateBadge`, the store badge's size and margin; opensu's own, since iiSU's grid draws
    no badges);
  - its details page reads "Installed, update available" and has Update after Play;
  - its tile menu has Update after Launch.
- Update takes one press (an update replaces what the store already holds): "updating <title>", or
  "cannot update <title>: <why>" (`app::UpdateRequest`, shared by both).
- Refused, by the rules `Installs` shares with uninstall: a ROM ("openSU does not update ROMs"), a
  game not installed, the game whose launch runs, any install, update or uninstall under way
  ("<title> is still updating"; installs and uninstalls are refused while an update runs). Also:
  no update known ("<title> is up to date"), Steam not running, a GOG game with no record.
- The Downloads page lists an Epic or GOG update as "Updating" (`/state` spelling `downloading`)
  with Cancel, then Finished, Failed or Cancelled. A Steam update's row is Steam's queue row once
  Steam lists it; the job's own row goes when it ends, so nothing reads Finished before Steam has
  run it.
- On success: a toast ("<title> updated", or "<title> update queued in Steam") and a catalog
  reload, which reads the new versions; a failure says why.

## Per store

- **Epic.** `library/epic:Provider::list` parses the installed `version` and `platform` from
  `list-installed --json` and `asset_infos.<platform>.build_version` from `list --json` (both
  already run; `vars(Game)` turns `asset_infos` into a dict, `legendary/models/game.py`). An update
  is there when both are known and differ, legendary's own rule for the `--check-updates` column
  (`legendary/cli.py:list_installed`: `versions[app] != game.version`). `EpicInstallJob` with
  `setUpdate(true)` runs `legendary update <app> --update-only -y --skip-sdl`: `update` is an alias
  of `install` (`cli.py` parser aliases), `--update-only` refuses a game that is not installed,
  and legendary keeps the install's path, platform and saved install tags (`install_game`), so no
  `--base-path` is passed.
- **GOG.**
  - The build: `gog-installs.json` records `build` beside path and platform, read after a download
    or update from what gogdl left (`gog::installedBuild`, the way `gogdl import` reads it,
    `gogdl/imports.py:get_info`/`load_game_details`): for Windows `buildId` of `goggame-<id>.id`,
    else of `goggame-<id>.info`; for Linux the version on `gameinfo`'s second line.
  - Records made before this: the provider reads the build from those files at each listing (no
    write-back, so the install job stays the records' one writer). A folder that has neither file
    is unknown: no request, no update shown.
  - The latest (`library/gog_builds`), one request per installed game per listing, with the
    sign-in's Bearer header as gogdl's session sends it:
    - Windows: `https://content-system.gog.com/products/<id>/os/windows/builds?generation=2`
      (`gogdl/constants.py` `GOG_CONTENT_SYSTEM`; `dl/managers/manager.py:get_builds`). gogdl
      downloads the first build whose `branch` is null, else the first (`setup_download_manager`);
      its `build_id` is compared. Heroic's import matches a `goggame` `buildId` against the same
      list's `build_id` (`src/backend/storeManagers/gog/library.ts`), so they share one namespace.
    - Linux: gogdl 1.3.1 does not use the content system for Linux; it installs from GOG's
      installers (`dl/managers/linux.py:setup`, `api.get_item_data(id, ['downloads', ...])`, i.e.
      `https://api.gog.com/products/<id>?expand=downloads`). The only Linux installer, else the
      English one (`find_matching_installer`), gives `version`. Heroic checks Linux updates the
      same way (`checkForLinuxInstallerUpdate`: `installer.version === version`).
    - A request that fails or an answer of another shape is logged and shows no update; the
      listing stands.
  - The update: `GogInstallJob` with `setUpdate(record)` runs `gogdl --auth-config-path <file>
    update <id> --platform <recorded> --path <recorded folder>`. gogdl appends its folder name only
    for `download` (`manager.py`: `should_append_folder_name = command == "download"`), so the
    recorded folder is passed as it is. The token goes in and out as for an install. The record
    keeps its folder and platform and takes the new build.
  - Sources read through `gh api` at tag v1.3.1 of `Heroic-Games-Launcher/heroic-gogdl` and the
    default branch of `Heroic-Games-Launcher/HeroicGamesLauncher`; nothing was run.
- **Steam.** Steam updates itself; openSU only marks and queues.
  - Pending: `library/steam` sets `updateAvailable` for an installed app whose manifest
    `StateFlags` has StateUpdateRequired (2), the bit `Library::installed` already treats as "not
    ready to run". Evidence on this machine: `appmanifest_1628350.acf` reads `StateFlags 6`,
    `buildid 24599756`, `TargetBuildID 25607578`, `UpdateResult 0`; every other manifest reads
    `StateFlags 4` with `TargetBuildID` equal to `buildid`.
  - Update: `steam::DownloadControl::moveToTop` evaluates
    `SteamClient.Downloads.ResumeAppUpdate(<app>, "0")` through `steam::DevTools` (a non-numeric
    id asks nothing), in `app::SteamUpdateJob`, an `InstallJob` that ends once Steam took it.
  - Steam's own UI (the client's `steamui/chunk~2dcc5aaf7.js`): `QueueAppUpdate` appears once,
    in the downloads page's item, as a callback whose value is discarded, so this build of Steam
    never calls it. The library's Update and Resume actions call
    `SteamClient.Downloads.ResumeAppUpdate(appid, clientid)` (`DownloadControl::moveToTop`), which
    starts the app's update at once, so openSU's Update makes the same call.
  - Steam's per-app auto-update policy (`SteamClient.Apps.SetAppAutoUpdateBehavior`) is left to
    Steam.

## Owners

- `library/game.hpp`: `Game::updateAvailable`.
- `library/epic`, `library/steam`, `library/gog` with `library/gog_builds` (offered build,
  `behind`) and `library/gog_installs` (`Installed::build`, `installedBuild`).
- `steam/download_control:DownloadControl::moveToTop`.
- `app/epic_install_job` (`setUpdate`), `app/gog_install_job` (`setUpdate`, build recorded),
  `app/steam_update_job:SteamUpdateJob`.
- `app/installs:Installs::update`, `Work::Update`, the shared `refusal`, `busy`.
- `app/update_request:UpdateRequest`, used by `app/details_controller` and
  `app/context_menu_controller`; `app/panel_flow:PanelFlow::workEnded` (toast and reload).
- `ui/game_details` (`detailsUpdateId`, the Status fact), `app/context_menu_controller`
  (`menuUpdateId`), `ui/tile_geometry:updateBadge`, `ui/tile_painter` (`TileVisual::update`,
  `paintUpdate`), `ui/shell:fillContent`.
- `app/downloads_board` (`DownloadRow::update`, the Steam update joining Steam's row),
  `app/downloads_controller:entryOf` ("Updating").

## Tests

- `tests/library/epic_test`: fixtures in legendary 0.20.35's JSON shape
  (`tests/library/fixtures/legendary/`): older version has an update, equal has none, the installed
  platform's build is the one compared (a Mac install), no offered build is none.
- `tests/library/gog_test`: a fake GOG serving the builds and product answers: a recorded Linux
  build behind the installer version, an old record read from `goggame-2.info` behind the build on
  no branch (a branch build first in the list), equal builds with no update, no request for an
  unknown build, a failing answer leaving the listing whole.
- `tests/library/gog_install_test`: `installedBuild` from `.info`, `.id` winning, another game's
  files, `gameinfo`; a record with and without its build.
- `tests/library/steam_test`: a fixture manifest with `StateFlags 6` and the build ids above has
  an update; fully installed and not installed ones have none.
- `tests/steam/devtools_test`: the exact `ResumeAppUpdate` expression against
  `tests/support/fake_steam.hpp`; a bad id asks nothing.
- `tests/app/update_job_test`: through `Installs` with stub `legendary` and `gogdl`: the refusals
  (ROM, not installed, running, up to date, busy for install, uninstall and a second update), the
  state's `Work::Update`, the Epic argv and the next install's argv, a GOG install recording its
  build, the GOG update argv on the recorded folder and the new build recorded, no GOG record,
  Steam refused without Steam, `SteamUpdateJob` asking a fake Steam and failing without one.
- `tests/app/downloads_board_test`, `downloads_controller_test`: update rows, Cancel, "Updating",
  the Steam update leaving its row to Steam's queue.
- `tests/app/details_page_flow_test`, `library_tools_test`: Update only with an update, after
  Play / Launch, one press, the refusal toast.
- `tests/ui/tile_geometry_test`: the badge's size, corner and clearance of the store icons.
- Store run of `tools/screenshots --parity`: the stand-in `legendary` installs its game a build
  behind; `update-badge` (its tile in the Epic launcher) and `details-update` (Update focused).

## Not done

- Unverified against real stores: an Epic update, a GOG update (and whether every Linux installer's
  `version` matches its `gameinfo`), a real Steam update started with `ResumeAppUpdate`. Live checks need a
  signed-in store with an outdated game on this machine or the VM (`docs/vm-harness.md`).
- No "Updates available" list or "Update all" on the Downloads page.
- GOG DLC and branches: the update follows the public build only, as an install does.
