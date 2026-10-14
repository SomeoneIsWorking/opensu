# Downloads page: active downloads and their progress

Status: open.

Requested: "downloads menu showing active downloads and progess".

## Current state

- **Steam:** `steam/downloads:DownloadQueue` reads `SteamClient.Downloads` items and overview once a
  second through DevTools. `steam::Download` carries app id, progress, active, paused,
  installing-or-update and seconds left. `steam/client:Client::downloads()` exposes the last read.
  It is used for the Steam badge's progress ring (`app/launcher_status:launcherBadges`) and for the
  `Updating · N%` launch gate (`launch/handoff.cpp`, `Client::updateProgress`). No screen lists the
  queue.
- **Epic, GOG and Steam installs started from openSU:** `app/installs:Installs` runs one install at
  a time.
  - `InstallJob::take` hands out the latest report once.
  - `app/panel_flow:PanelFlow::service` consumes it for the launch panel. B hides the panel (`{"B",
    "Hide"}`), after which the progress is invisible until the success or failure toast.
  - There is no cancel. `InstallJob` stops its thread only in `halt()` at destruction, although
    `CliInstallJob::run` already passes a `stop_token` to `launch::runStreaming`.
  - There is no queue: a second install is refused while one runs.
- **Emulators and cores:** `app/emulator_downloads:EmulatorDownloads` runs one download at a time,
  with `progress()` (bytes received and total) and `cancel(id)`. Progress shows on the details page
  and in Settings › Emulators.
- **Artwork:** `artwork/artwork_fetcher` queues per-game stages (`stageOf`). It is background work
  and is not presented as downloads.

## iiSU reference

iiSU has no Downloads screen. Background work appears as Active Tasks:

- `bw.java` keeps `active_task_keys` and `task:<key>:title`, with keys such as
  `romm_download:<hash>` and `update_install:apk`.
- The "Active Tasks" menu is `ye4` and the "Active Task: %1$s" line is `bk2`, with "Cancel current
  task" and "Progress" strings. The queue limit message is "Task queue full" (`u39`).
- These sit inside the notification center (`fn4`, tabs "iiSU Notifications" / "System
  Notifications"), opened with R2 (`tl4.java:147`, `docs/reference/iisu/screens.md` §2). RomM ROM
  downloads report "Downloading %1$s (%2$d%%)" (`g37.java`).

openSU dropped the bell and the R2 overlay (project-state baseline), so the page is openSU's own.
It follows iiSU's content: one row per task with its title, progress and a cancel.

## Gap

No screen aggregates downloads. Epic, GOG and Steam-wizard installs cannot be watched once the panel
is hidden, and cannot be cancelled. Steam's queue cannot be paused, resumed, reordered or removed
from openSU. The install job hands its state out once, so a second reader cannot see it.

## Proposed owners and design

1. `app/install_job`: add `status()`, a non-consuming snapshot (title, line, fraction, state), and
   `cancel()`, which requests the thread's stop. `CliInstallJob` already stops on its token. Steam's
   job cancels the wizard or removes the download (see 3). `Installs` forwards both.
   `PanelFlow` keeps using `take`.
2. Queue (optional, see the open questions): `Installs` holds a FIFO of pending games and starts
   the next when one finishes. This matches Steam's own queue, which openSU then only mirrors.
3. `steam/download_control.*` (new, beside `steam/downloads`): `pause`, `resume`, `remove` and
   `moveToTop` through `SteamClient.Downloads.PauseAppUpdate(appId, clientId)`, `ResumeAppUpdate`,
   `RemoveFromDownloadList` and `SetQueueIndex`. These are the calls Steam's own downloads page
   makes (`~/.steam/steam/steamui/chunk~2dcc5aaf7.js`). Also `EnableAllDownloads(bool)` for
   pause all.
4. `app/downloads_board.*` (the model, no UI) builds one list of `DownloadRow` values each frame
   from the sources:
   - `Client::downloads()`, with titles from the catalog by app id. This includes Steam's
     auto-updates and queued and paused items.
   - `Installs::status()`.
   - `EmulatorDownloads::progress()`.

   Each `DownloadRow` holds source, game id, title, kind (install, update, emulator, core), state
   (active, queued, paused, failed), fraction, bytes and seconds left. Artwork is not listed.
5. `ui/downloads_page.*` and its painter: a `ui/page_panel` page like Devices. Each row shows cover,
   title, store badge, a bar with percent and time left, and A for the row's actions (pause/resume
   and move to top for Steam; cancel for all). An empty state says "No downloads". The controller
   is `app/downloads_controller.*`.
6. Entry points:
   - a "Downloads" row in the Guide menu (`ui/guide_menu` `GuideAction::Downloads`, home and in
     game);
   - a click on a launcher badge that shows a progress ring;
   - the `Downloads` chord action (`guide-chord-configuration.md`).

   `/state` adds a `downloads` array for the control channel.

## Tests

- `downloads_board` against fakes of each source.
- `install_job` cancel for the CLI jobs (a stub `legendary` killed mid-run leaves no record).
- `download_control` against `tests/support/fake_steam.hpp`.
- `downloads_controller` rows and actions.
- A hidden-run capture under `scratch/downloads-page/`.

## Open questions

- Should Epic and GOG installs queue (proposed), or stay one at a time with a refusal?
- Should finished downloads stay listed as a history for the session, like iiSU's notification
  history, or disappear when done?
