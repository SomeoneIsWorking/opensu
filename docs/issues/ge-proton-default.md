# GE-Proton is the default Wine runner, kept up to date by openSU

Status: partial. Built and shown in the hidden product (below); a real GOG or Epic Windows game
launched through it is not. Project state S038.

Requested: "Wine default being system is not good. Can we make it GE-Proton and update it
automatically, it will use the last installed version and when new one is installed it will start
using that one and it will delete not the previous one but the one before previous". Then: "We can
add a list of installed runtimes and show how many games using them".

## How a runner runs a game

- Plain Wine (system `wine`): unchanged, `--wine <binary> --wine-prefix <prefix>` to gogdl or
  legendary.
- A Proton build (openSU's GE-Proton, or a Proton found in Steam's folders): through umu-launcher,
  the documented way to run Proton outside Steam. gogdl and legendary get `--no-wine --wrapper
  '<umu-run>'`; the launch environment gets `WINEPREFIX=<prefix>`, `PROTONPATH=<build folder>`,
  `GAMEID=umu-0`, `STORE=gog|egs` and `UMU_FOLDERS_PATH=<data>/wine`. With an absolute
  `PROTONPATH` umu downloads no Proton; with `UMU_FOLDERS_PATH` the Steam Linux Runtime it fetches
  (steamrt4 for GE-Proton 11, about 1.3 GB) lands in `<data>/wine/umu/`, not in the player's
  `~/.local/share/umu` or Steam's `compatibilitytools.d`. Proton runs inside that runtime with its
  DXVK/VKD3D setup, unlike the old path that used a Proton build's bare `files/bin/wine`.
- One rule picks a game's runner (`library::wine::runnerOf`): the game's pin
  (`settings.json` `wineRunnerPins`), else the runner picked in Settings › Wine (`wineRunner`),
  else the newest GE-Proton openSU installed. The launch and Settings' game counts both use it.

## Built

- `library/ge_proton`: GE releases from `GloriousEggroll/proton-ge-custom` (the
  `-x86_64`/`-aarch64` tarball for this CPU, or the plain one of older releases), the SHA-512 from
  the release's `.sha512sum`, builds under `<data>/wine/runners/GE-Proton<a>-<b>` ordered by
  version, and the rotation: everything past the newest two is removed unless in use.
- `library/umu_launcher`: umu-launcher's zipapp release, checked against the SHA-256 GitHub
  publishes for the asset (the release has no checksum file), under
  `<data>/wine/umu-launcher/<version>/umu/umu-run`; only the newest is kept, and none is removed
  while a game runs. It needs `python3` on the host.
- `library/runtime_archive`: download, size and digest check, `tar -xf` into
  `<into>/.partial`, the archive's one folder renamed into place under the build name. A failure
  or cancel removes the staging folder and leaves the installed builds alone. SHA-512 is Lucent's
  (`lucent::content::sha512_file`, added for this).
- `app/wine_runners`: the check runs at start and every 24 h while GOG or Epic is in force (an
  hour after a failed start), on "Check for GE-Proton now", and when a Windows launch or Test the
  runner finds no GE-Proton. With no GE-Proton installed only those last two download it (565 MB);
  the others look up the latest release ("GE-Proton11-11 · downloads on first use"). A scheduled
  check that cannot read the release (HTTP 403, GitHub's rate limit) logs it once and waits for
  the next scheduled check. Sizes are measured again when a game ends, which is when the Steam
  Linux Runtime umu fetched on a first Proton launch shows up. After a successful check the rotation runs on the main loop: builds past the newest
  two are renamed into `<data>/wine/.trash` and removed on a thread; a build kept back says why
  ("Kept: 1 game pinned to it", "Kept: a game runs on it", "Picked for every game"). Progress and
  Cancel on the Downloads page (`wine:ge-proton`, `wine:umu-launcher`, "Wine runner").
- A Windows launch with no GE-Proton yet is held: the launch panel shows "Getting GE-Proton11-11 ·
  45%", B drops it, and the game starts when the install ends (`WineSettings::serviceHeld`); the
  panel stays up while held (`ShellApp::serviceRequests` counts `WineSettings::holding`).
- Settings › Wine: Wine runner (Latest GE-Proton (11-11), each GE build, other runners found,
  System Wine), Check for GE-Proton now with the last outcome, Test the runner (`cmd /c ver` in
  `<data>/wine/test-prefix` through the same invocation a launch uses), then "Runners and
  runtimes": every runner with where it lives (kept up to date by openSU, found in Steam's
  folders, system), size for openSU's builds, and how many games use it; umu-launcher and the
  Steam Linux Runtime with their sizes. Each opens a page: where, size, why kept, the games using
  it (A unpins a pinned one, pins of games the library does not list now included) and "Pin a
  game to it" for the other Windows games.
- Migration: an empty `wineRunner` (the old Automatic) now means the newest GE-Proton; an explicit
  pick (system `/usr/bin/wine`, a Steam Proton) stays as it was. Nothing is rewritten.
- Prefixes made by plain Wine: Proton upgrades a prefix in place and umu links
  `drive_c/users/steamuser` to the player's user folder, so saves stay where games look. Before a
  Proton build first runs in a prefix plain Wine made (registry files, no `pfx` link), openSU
  copies its `drive_c/users`, `drive_c/ProgramData` and `*.reg` once to
  `<prefix>.system-wine-saves` (`library::wine::keepWineSaves`). Going back to plain Wine on a
  prefix Proton touched is not handled specially.

## Shown in the product (2026-10-27, `opensu --hidden`, scratch XDG dirs)

Frames and logs in `scratch/ge-proton/`. `OPENSU_GE_PROTON_RELEASE` named an older release's API URL
for the first two runs and the fifth.

1. GE-Proton11-9: downloaded with its row on Downloads (`downloads-ge-proton11-9.png`), "matches
   its published SHA-512 30160946…", unpacked; umu-launcher 1.4.4 "matches its published SHA-256
   eb590691…". Test the runner: umu fetched steamrt4, "Proton: Upgrading prefix from None to
   GE-Proton11-9", then `Microsoft Windows 10.0.19045` (`logs/run1b.log`).
2. GE-Proton11-10 installed; two builds kept. Runner set to System Wine, Test made the test
   prefix with plain Wine; a save file was put in `drive_c/users/bhamil/AppData/Roaming`; runner
   back to Latest GE-Proton, Test: "was made by plain Wine; its saves are copied to
   …test-prefix.system-wine-saves", "Upgrading prefix from None to GE-Proton11-10", Windows
   answered; `drive_c/users/steamuser -> bhamil` and the save reads back unchanged
   (`logs/run2.log`, `settings-wine-two-builds.png`).
3. With `gog:1207658930` pinned to GE-Proton11-9 in `settings.json`, the latest GE-Proton11-11
   installed (`downloads-ge-proton-latest.png`) and the rotation kept 11-9: "GE-Proton11-9 stays
   past the newest two: Kept: 1 game pinned to it" (`settings-wine-kept.png`,
   `runtime-page-ge-proton11-9.png`).
4. Unpinned on GE-Proton11-9's page (`runtime-page-pinned.png`), Check now: "rotation removes
   GE-Proton11-9 (keeping GE-Proton11-11 and GE-Proton11-10)"; the runners folder holds 11-10 and
   11-11 and the trash is gone (`settings-wine-after-rotation.png`). Test now runs
   "Upgrading prefix from GE-Proton11-10 to GE-Proton11-11" and answers (`logs/run4.log`).
5. Test the runner pressed while GE-Proton11-9 downloads is refused with a toast, "A Wine runner
   download or test is running; try again when it ends" (`settings-wine-test-refused.png`).

The player's `~/.local/share/umu` and `~/.local/share/Steam/compatibilitytools.d` were not touched
(mtimes unchanged).

The exact wrapper and environment a GOG launch gets were also run by hand through the real
`gogdl launch <dir> 1 --platform windows --no-wine --wrapper '<umu-run>'` on a scratch game folder
holding `cmd.exe`: gogdl ran umu-run with the exe, Proton started it and everything exited 0
(`logs/gogdl-wrapper.log`); cmd's own output did not reach that log.

On the player's own setup (installed data dirs, `--hidden` inside a headless Gamescope;
`scratch/ge-proton-real/ge2/`), with GE-Proton11-11 and `wine/umu` moved aside for the run and
restored after: the start check only looked up GE-Proton11-11 (`01-settings-wine-start.png`, no
Steam Linux Runtime row); Play on Nuclear Throne (Epic) held the launch on the panel, "Getting
GE-Proton11-11 · 3%" (`07-held-launch-later.png`), the SHA-512 matched, the game started through
umu-run and showed its window; Settings › Wine during the game counted it on umu-launcher and still
had no runtime row (`11-settings-wine-in-game.png`); after Close game the row "Steam Linux Runtime
(steamrt4) · 1.3 GB · 1 game" is there (`14-settings-wine-after-game.png`). A start check against a
release URL answering HTTP 403 logged one `[wine:warn]` line, no toast, no Downloads row
(`x403/run.log`).

## Open

- A system Wine's version is not shown (its name is "System Wine"); Steam's Proton builds show no
  size (not measured, too many files to walk on the loop).
- `GAMEID` is `umu-0` for every game: umu's per-game fixes (protonfixes by store id) are not
  looked up.
- The Steam Linux Runtime is fetched by umu itself on the first Proton run, without progress in
  openSU; umu keeps it current on later runs.
