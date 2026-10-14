# Uninstall games

Status: open.

Requested: "ability to uninstall games".

## Current state

- Nothing uninstalls a store game. `ui/details_page` `DetailsAction` has Launch, Emulator,
  DownloadEmulator, Hidden and Back. The context menu (`app/context_menu_controller`) has Launch,
  Install, Details, Hide, Open, Refresh and Sign in.
- **Steam:** `steam/install_wizard` drives `SteamClient.Installs` for installs only. Steam's own UI
  removes a game with `SteamClient.Installs.OpenUninstallWizard([appId], true)` after its own
  confirm dialog. It also registers `Installs.RegisterForShowConfirmUninstall` and
  `RegisterForShowFailedUninstall`, so Steam may raise its own popups
  (`~/.steam/steam/steamui/chunk~2dcc5aaf7.js`). Unmeasured.
- **Epic:** legendary 0.20.35 has `legendary uninstall <app> [-y] [--keep-files]
  [--skip-uninstaller]`. Its Windows uninstaller is only run on `os.name == 'nt'`
  (`legendary/cli.py:uninstall_game`), so on Linux it deletes the files and the database entry.
- **GOG:**
  - gogdl has no uninstall command.
  - openSU's `library/gog_installs:InstallRecords` has `find`, `all` and `add`, but no removal.
  - A Windows install also has a Wine prefix at `Paths::prefixes/<id>`.
  - project-state S007: "No uninstall … a deleted game folder stays recorded."
- **Emulators and cores** downloaded by openSU are already removable (two presses,
  `EmulatorDownloads::remove`, S026/S031).
- **ROMs** are the player's own files under their ROM roots. openSU never writes there.

## iiSU reference

iiSU uninstalls nothing. Its dex has no `ACTION_DELETE`, `UNINSTALL_PACKAGE` or
`PackageInstaller.uninstall`. For apps it offers only "Hide APP" and "Delete current linked app",
which unlinks a shortcut. For ROMs it deletes only local copies it downloaded from RomM ("Delete
ROM" → "Deleted local ROM: %1$s", `ug0.java`; `deleteAllRommRoms`, `sg0.java`). The design
therefore follows iiSU's line for ROMs: openSU does not delete files it did not put there.

## Gap

There is no uninstall for Steam, Epic or GOG, no confirmation UI, no GOG record removal, and no
catalog reload after a removal.

## Proposed owners and design

1. `steam/uninstaller.*` (beside `install_wizard`): `uninstall(appId)` through
   `OpenUninstallWizard([appId], true)`. It then waits until the manifest is gone, with the Steam
   queue and `Client::installed` as the signal. If Steam raises its confirm dialog anyway (to be
   measured), the uninstaller confirms it through the same JS path Steam's dialog uses, rather than
   showing a Steam window. The cancel path is Steam's own.
2. `app/uninstall_job.*`: a store-neutral job on a worker thread, like `InstallJob`, with three
   store implementations:
   - Steam, through `steam/uninstaller`.
   - Epic, running `legendary uninstall <app> -y` through `launch::runStreaming`. The failure is
     taken from the ERROR/CRITICAL line (`library/install_log`).
   - GOG, which removes the recorded folder, then the record. The folder is removed only if it lies
     under the root openSU gave gogdl (`GogInstallJob::gameRoot(id)`, under the resolved install
     folder), and a record pointing anywhere else is only forgotten. The Wine prefix is removed
     with it (see the open questions). `InstallRecords::remove(id)` is new.

   `app/installs` refuses an uninstall while that game installs or runs. The one-at-a-time rule
   applies to installs and uninstalls together.
3. UI:
   - The details page gains `DetailsAction::Uninstall` for an installed store game. It is a
     two-press confirm with "Press A again to uninstall <title>", the pattern of Settings ›
     Emulators' Remove.
   - The game's context menu gains Uninstall, which opens the same confirm.
   - On success: a toast and a catalog reload (`hooks_.reloadCatalog`). On failure: a toast with
     the reason.
   - A title owned in several stores names the store being removed.
4. ROMs: no uninstall (see the open questions). A ROM's details page keeps Hide.

## Tests

- `uninstall_job` per store against stubs: legendary argv and failure line, Steam through
  `fake_steam` and a manifest removed, GOG folder and record removed, and a record outside the
  root forgotten without deleting.
- `gog_installs` remove.
- Details page two-press flow.
- Refused while the game installs or runs.

Live verification needs a signed-in store on this machine, or the VM (`docs/vm-harness.md`).

## Open questions

- Should a GOG uninstall delete the Wine prefix, which may hold saves (proposed: yes, after the
  confirm names it), or keep it?
- Should ROMs ever be deletable from openSU? iiSU deletes only ROMs it downloaded, and openSU
  downloads none.
- Should Epic's `--keep-files` (remove from legendary's list, keep files) be offered?
