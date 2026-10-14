# Downloads page: active downloads and their progress

Status: partial. Built and verified by unit tests, a hidden run and the `downloads` parity scene
against stand-ins; real Steam pause, resume, reorder and remove, and a real Epic or GOG cancel, have
not been seen.

Requested: "downloads menu showing active downloads and progess".

## Decisions

- Epic and GOG installs stay one at a time; a second is refused while one runs, as before.
- Finished, failed and cancelled downloads stay listed, latest first, until openSU quits.

## Behaviour

- One page lists every download openSU knows of: Steam's queue (its own auto-updates included),
  the store install openSU runs (Epic, GOG, Steam's install wizard) and the emulator or core
  download. Each row shows the title, the source, the state (Queued, Downloading, Paused,
  Installing, Finished, Failed, Cancelled), the figures the owner gives (percent; bytes and rate
  for Steam and emulators; time left for Steam), a progress bar while measured, and its buttons.
- Order: downloading and installing, then paused, then queued in Steam's order, then the ended
  ones, latest first. "No downloads" when empty.
- Buttons, by owner:
  - Steam, the active download: Pause (pauses Steam's queue, as Steam's own button does).
  - Steam, the paused head: Resume, Remove. A paused download out of the queue: Download now,
    Remove.
  - Steam, queued: Download now, Move up (from place 2 on), Move down (not the last), Remove.
  - Epic, GOG, Steam's wizard before Steam queues the download, an emulator or core: Cancel.
  - Ended rows have none.
- Up and Down move between rows, Left and Right between a row's buttons, A presses, B closes. A
  click focuses a row or presses a button.
- Entry points: the Guide menu's Downloads row (home and over a game), a click on a launcher badge
  showing a progress ring, and the Downloads chord action (no chord by default; Settings › Guide
  chords binds one).
- `/state` has `downloadsOpen` and `downloads`, one `{key, title, source, state, percent}` per row.

## Steam's own calls

From Steam's downloads page (`~/.steam/steam/steamui/chunk~2dcc5aaf7.js`), with this machine's
client id "0":

- Pause and resume are `SteamClient.Downloads.EnableAllDownloads(false|true, "0")`; the page has
  no per-item pause for the active download. `PauseAppUpdate` is what a drag out of the queue
  calls, not the pause button, so the earlier proposal to pause with it was wrong.
- Download now / Move to top is `ResumeAppUpdate(appid, "0")`; a drag within the queue is
  `SetQueueIndex(appid, index, "0")`.
- Remove is `RemoveFromDownloadList(appid, "0")`; removing the paused head (`queue_index` 0 while
  the overview is paused) then resumes the queue. The page hides Remove on the active download.
- Bytes are `update_type_info[k].progress[2].bytes_in_progress/bytes_total` summed per item (stage
  2 is the transfer); the active download's live bytes, `update_network_bytes_per_second` and
  `update_state` come from the overview.

## Owners

- `steam/downloads:parseDownloads` reads `queue_index`, bytes, rate and `update_state`.
- `steam/download_control:DownloadControl` makes the calls above; app ids must be numeric.
- `app/install_job:InstallJob::status()` and `cancel()`: a non-consuming state per start and a stop
  that ends the job with a cancelled report (the store's process is stopped through its
  `stop_token`). `app/installs:Installs::status()` and `cancel()` forward them.
- `app/emulator_downloads:EmulatorDownloads::lastFinished()` keeps how the last download ended.
- `app/downloads_board:DownloadsBoard` merges the sources each frame into rows, states, order and
  actions. Pure.
- `app/downloads_controller:DownloadsController` feeds the board, runs a pressed button on the
  row's owner (Steam calls on a worker, one at a time) and fills the page.
- `ui/downloads_page`, `ui/downloads_page_painter`, `ui/downloads_panel`: the page's state and
  geometry, its drawing, and its fade and pointer targets.

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

## Tests

- `steam_devtools`: the new fields parsed; each `DownloadControl` expression exact against
  `tests/support/fake_steam.hpp`, removing the paused head resumes, a bad app id asks nothing.
- `install_job`: cancel while asking for a licence, a stub `legendary` cancelled mid-download ends
  cancelled (not failed) and its process is gone, cancel through `Installs`.
- `downloads_board`: merging, order, state mapping, Steam actions, install endings, the Steam
  wizard joining its queue row, Steam unreadable, removal, emulator endings.
- `downloads_controller`: Cancel reaches `Installs` (stub `legendary`) and `EmulatorDownloads`
  (fake release host); each Steam button sends its call to a fake Steam; a refused call is told.
- `downloads_page`, `control_channel`, `breadcrumb_trail`, the Guide menu, shortcut and pointer
  tests.
- Parity scene `downloads` (`tools/screenshots`, `--parity`): a run of its own with a stand-in
  `legendary` whose install logs 42% and waits.

## Not done

- No cover art or store badge on a row; the source is text.
- Epic and GOG rows show percent only: legendary's and gogdl's byte and ETA figures are not parsed.
- A tile has no progress badge, so the badge entry point is the launcher badge's ring; A cannot
  reach the top bar's badges.
- Unverified: Steam's buttons against a real Steam client, a real Epic or GOG cancel, the page over
  a game in Gamescope.
