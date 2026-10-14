# Uninstall games

Status: partial. Built and verified by unit tests through the shipping owners and the
`details-uninstall` and `downloads-uninstalling` parity scenes against a stand-in `legendary`; no
real Steam, Epic or GOG uninstall has been run.

Requested: "ability to uninstall games".

## Behaviour

- An installed Steam, Epic or GOG game has Uninstall on its details page (after Hide) and in its
  tile menu (last). A ROM and a game that is not installed have none.
- First press: openSU asks `Installs` what would go. A refusal is a toast ("cannot uninstall
  <title>: <why>") and arms nothing; the tile menu closes. Otherwise the button arms:
  - details page: the button reads "Press A again to uninstall" and an "Uninstall" row, first on
    the page, says what goes;
  - tile menu: the entry reads "Press A again: <what goes>".
  - What goes, which names the store, so a title owned in several stores says which copy:
    "Steam uninstalls the Steam copy", "legendary deletes the Epic copy", "deletes the GOG copy at
    <folder>", "forgets the GOG copy; <folder> is kept".
- Moving focus (pad, keyboard or pointer), closing or reopening lets go of the armed button.
- Second press: the uninstall starts ("uninstalling <title>"), on a worker. The Downloads page
  lists it as Uninstalling with the store's line ("Steam is removing it", "legendary is removing
  it", "Removing its files"), then Uninstalled or Failed; it has no buttons, since a stopped
  uninstall leaves half a game.
- On success: a toast ("<title> uninstalled", or "<title> forgotten; <folder> is kept") and a
  catalog reload, so the game leaves Home's installed shelf and its page offers Install. On
  failure: "cannot uninstall <title>: <reason>".
- Refused: a ROM ("openSU never deletes ROMs"), a game not installed, the game whose launch runs,
  any install or uninstall under way ("<title> is still installing"/"uninstalling"; installs are
  refused while an uninstall runs too), a Steam game while Steam is not running, a GOG game with
  no record.
- `/state`: `armed` while a details button or a menu entry waits for its second press; the
  uninstall's row in `downloads` with state `uninstalling`, then `uninstalled`.

## Per store

- **Steam:** `steam/uninstaller` evaluates `SteamClient.Installs.OpenUninstallWizard([<app>],
  true)` through `steam::DevTools`, the call Steam's own library menu makes
  (`~/.steam/steam/steamui/chunk~2dcc5aaf7.js`); a non-numeric id asks nothing (`isAppId`). The
  job then polls every second until no library holds the app's manifest (`Client::hasManifest`),
  failing after two minutes with "Steam did not remove it".
- **Epic:** `legendary uninstall <app> -y` through `launch::runStreaming`; the failure is the
  first ERROR/CRITICAL line (`library/install_log`), else the exit status. On Linux legendary runs
  no Windows uninstaller (`legendary/cli.py:uninstall_game`); it deletes the files and its record.
  `--keep-files` is not offered.
- **GOG:** gogdl has no uninstall. The game's root is `GogInstallJob`'s, now
  `gog::Paths::gameRoot(folder, id)`: `<install folder>/<id>`, or `<data>/gog-games/<id>` with no
  folder set, with the GOG install folder as it is resolved when the uninstall starts.
  `gog::ownedFolder` takes the recorded folder with symlinks resolved and accepts it only when it
  lies strictly inside the install folder (symlinks resolved) joined with `<id>` as it is, so a
  record outside the root, a symlink in the root pointing out, a root that is itself a symlink
  out, or a missing folder is only forgotten. An accepted folder is deleted, then the root if it
  is empty, then the record (`InstallRecords::remove`). A game installed into a GOG folder that
  has since been changed is therefore only forgotten.
- No Wine prefix is touched: `<data>/gog-prefixes/<id>` is outside every game root, and the shared
  prefix of `windows-games-wine-prefix.md` will be too.
- ROMs are the player's own files; openSU deletes none (iiSU deletes only ROMs it downloaded from
  RomM, and openSU downloads none).

## Owners

- `steam/uninstaller:Uninstaller`, `steam/client:Client::uninstaller()` and `hasManifest`,
  `steam/steam_json:isAppId` (shared with the installer and `DownloadControl`).
- `library/gog_installs`: `Paths::gameRoot`, `InstallRecords::remove`, `ownedFolder`.
- `app/uninstall_job:UninstallJob`, an `InstallJob` (thread, reports, `status`, `Phase::Done`);
  `removal` says what goes per store.
- `app/installs:Installs::removal`, `uninstall`, `busy`; `Status::work` tells an install from an
  uninstall; the `Playing` hook (`ShellApp`'s running launch) refuses a running game.
- `app/panel_flow:PanelFlow::service` takes the uninstall's end: toast and catalog reload.
- `app/uninstall_prompt:UninstallPrompt`: the two presses, shared by `app/details_controller` and
  `app/context_menu_controller`.
- `ui/details_page` (`DetailsAction::Uninstall`, `arm`, `shown`, `rows`) and `ui/context_menu`
  (`ContextAction::Uninstall`, `arm`, `shown`): the armed state, let go on every focus change.
- `app/downloads_board`: `DownloadState::Uninstalling` and `Uninstalled`.

## Tests

- `tests/library/gog_install_test`: `remove`, `gameRoot`, `ownedFolder` inside, outside, a symlink
  out, a symlinked root, a symlinked install folder, a missing folder, `..` out.
- `tests/steam/devtools_test`: the exact `OpenUninstallWizard` expression against
  `tests/support/fake_steam.hpp`, a bad id asks nothing, an unreachable Steam says so.
- `tests/app/uninstall_job_test`: Epic argv, failure line and missing legendary against stub
  scripts; GOG inside (folder and emptied root deleted, record gone, prefix kept), outside and
  symlinked out (only forgotten), the data-dir root, no record; Steam without Steam; `Installs`
  refusing a ROM, a game not installed, a running game, any uninstall during an install and any
  install or second uninstall during an uninstall, and its status naming the uninstall.
- `tests/ui/details_page_test`, `tests/ui/context_menu_test`: the button and entry only for an
  installed store game, arming, the armed label and row, every focus change letting go.
- `tests/app/details_page_flow_test`, `tests/app/library_tools_test`: two presses through the
  controllers, a refused first press.
- `tests/app/downloads_board_test`, `downloads_controller_test`, `control_channel_test`: the
  uninstall row's states, no actions, the page's line, `armed` and `uninstalling` in `/state`.
- Parity scenes (`tools/screenshots`, `--parity`, the store run): `details-uninstall` (armed),
  `downloads-uninstalling` (the stand-in's uninstall waits; the install the `downloads` scene left
  is cancelled first).

## Not done

- Unverified against a real store: whether Steam raises its own confirm
  (`RegisterForShowConfirmUninstall`) or failure popup for `OpenUninstallWizard(..., true)`; if
  it does, the job fails after two minutes and the popup is Steam's. A real Epic or GOG uninstall. Live checks need a
  signed-in store on this machine or the VM (`docs/vm-harness.md`).
- Epic's `--keep-files` is not offered.
- ROMs are never deletable from openSU.
