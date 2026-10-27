# GE-Proton is the default Wine runner, kept up to date by openSU

Status: partial. Built and shown in the hidden product (below), including an Epic game on the
player's setup under each fallback; a GOG game is not. Project state S038.

Requested: "Wine default being system is not good. Can we make it GE-Proton and update it
automatically, it will use the last installed version and when new one is installed it will start
using that one and it will delete not the previous one but the one before previous". Then: "We can
add a list of installed runtimes and show how many games using them". Then, rejecting a launch
that waited for the download: "I don't want waits like this, if GE-Proton not downloaded yet then
you can fallback to Steam Proton which falls back to what Lutris uses which falls back to system".

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
- One rule picks a game's runner (`library::wine::resolve`): the game's pin (`settings.json`
  `wineRunnerPins`), else the runner picked in Settings › Wine (`wineRunner`), else Automatic, the
  first that can run of: the newest GE-Proton openSU installed; the newest Proton in Steam's
  folders (`compatibilitytools.d` and `steamapps/common`, by the version in the name); what Lutris
  uses; system Wine. A Proton build counts only with a `umu-run` (openSU's, else one on PATH, else
  Lutris's `runtime/umu`). A launch never waits: it runs under what Automatic finds now, and picks
  GE-Proton up once the background download has installed it. The launch, Test the runner and
  Settings' game counts all use this rule.
- What Lutris uses (`library/lutris_wine`, read only): Lutris 0.5.22 here; its default Wine
  version is `ge-proton` (`lutris/util/wine/wine.py` `get_default_wine_version`), umu-run with
  `PROTONPATH=GE-Proton`, which umu resolves to the newest GE-Proton in
  `~/.local/share/Steam/compatibilitytools.d` and downloads when there is none. `runners/wine.yml`
  `wine: version:` overrides it (here `{}`, unset); `runners/wine` (empty here) holds Lutris's own
  builds. openSU takes Lutris's choice only when it is on disk; on this machine that is a GE-Proton
  in `compatibilitytools.d`, which the Steam step already takes, so the Lutris step adds nothing
  here.

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
- `app/wine_runners`: the check runs in the background at start and every 24 h while GOG or Epic is
  signed in (`library::isReady`; an hour after a failed start) and on "Check for GE-Proton now",
  and downloads the latest GE-Proton and umu-launcher when they are missing or older. A scheduled
  check that cannot read the release (HTTP 403, GitHub's rate limit) logs it once and waits for
  the next scheduled check. Sizes are measured again when a game ends, which is when the Steam
  Linux Runtime umu fetched on a first Proton launch shows up. After a successful check the rotation runs on the main loop: builds past the newest
  two are renamed into `<data>/wine/.trash` and removed on a thread; a build kept back says why
  ("Kept: 1 game pinned to it", "Kept: a game runs on it", "Picked for every game"). Progress and
  Cancel on the Downloads page (`wine:ge-proton`, `wine:umu-launcher`, "Wine runner").
- Settings › Wine: Wine runner (Automatic, its note naming what it resolves to now and why, e.g.
  "Now Steam's GE-Proton11-5-x86_64 (GE-Proton downloading)"; each runner found), Check for GE-Proton now with the last outcome, Test the runner (`cmd /c ver` in
  `<data>/wine/test-prefix` through the same invocation a launch uses), then "Runners and
  runtimes": every runner with where it lives (kept up to date by openSU, found in Steam's
  folders, system), size for openSU's builds, and how many games use it; umu-launcher and the
  Steam Linux Runtime with their sizes. Each opens a page: where, size, why kept, the games using
  it (A unpins a pinned one, pins of games the library does not list now included) and "Pin a
  game to it" for the other Windows games.
- Migration: an empty `wineRunner` means Automatic; an explicit pick stays as it was. Nothing is
  rewritten.
- Prefixes (`library::wine::readyPrefix`, for launches and the test):
  - Plain Wine to Proton: Proton upgrades the prefix in place and umu links
    `drive_c/users/steamuser` and the player's folder, so saves stay where games look. Before a
    Proton build first runs in a prefix plain Wine made (registry files, no `pfx` link), its
    `drive_c/users`, `drive_c/ProgramData` and `*.reg` are copied once to
    `<prefix>.system-wine-saves`.
  - Proton to plain Wine: never in the same prefix. Proton links its build's DLLs into
    `system32` (606 links in the real shared prefix, into the build that last ran it). With that
    build present, system Wine 11.20 ran `cmd /c ver` in a copy and the save stayed; with it gone
    (moved aside, as a rotation or uninstall does) system Wine's `wineboot` failed on every DLL
    with error 80 (file exists), `wow64.dll` did not load and Nuclear Throne exited before a window
    (`scratch/ge-proton-real/ge3/logs/B-no-ge-no-steam.log`). So plain Wine runs in
    `<prefix>.plain-wine`, which starts from a copy of the Proton prefix's `drive_c/users` and
    `drive_c/ProgramData`, made the first time; saves made there stay there.
  - Proton to another Proton: Proton relinks on a version change ("Upgrading prefix").

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

On the player's own setup (installed data dirs, `--hidden` inside a headless Gamescope, Steam not
running; `scratch/ge-proton-real/ge3/`), Nuclear Throne (Epic) on Automatic, runners moved aside on
their own filesystems for each run and restored after:

1. No openSU GE-Proton: Settings › Wine "Automatic · Now Steam's GE-Proton11-5-x86_64 (GE-Proton
   downloading)" (`frames/A2-no-ge-1-settings-wine.png`); Play started at once, "runs under
   GE-Proton11-5-x86_64 through umu-run", window shown, while GE-Proton11-11 downloaded behind it
   (`logs/A2-no-ge.log`).
2. No openSU GE-Proton and no Steam Proton: Lutris's default (GE-Proton latest) is not on disk, so
   Automatic is system Wine, "Now System Wine (GE-Proton downloading)"
   (`frames/B2-no-ge-no-steam-1-settings-wine.png`). The shared prefix is Proton's, so the saves
   were copied to `wine-prefix.plain-wine` and the game ran there under system Wine and showed its
   window (`logs/B2-no-ge-no-steam.log`). The run before the twin prefix existed is the failure
   above (`logs/B-no-ge-no-steam.log`); the shared prefix's registry was put back from the copy
   taken just before it.
3. Everything restored: "GE-Proton11-11 is the latest GE-Proton and installed", the game runs
   under it in the shared prefix and shows its window, its save unchanged (`logs/D-all-restored.log`).

"None → system" is case 2 on this machine: with no Proton the Lutris step finds nothing on disk.

Earlier (before the fallback chain), on the same setup: the Steam Linux Runtime row appears once a
game ends (`scratch/ge-proton-real/ge2/14-settings-wine-after-game.png`), and a start check
against a release URL answering HTTP 403 logged one `[wine:warn]` line, no toast, no Downloads row
(`ge2/x403/run.log`).

## Open

- A system Wine's version is not shown (its name is "System Wine"); Steam's Proton builds show no
  size (not measured, too many files to walk on the loop).
- The Lutris step is not distinct on this machine (see above); it is covered by
  `tests/library/wine_test.cpp` only.
- Saves made in `<prefix>.plain-wine` stay there; going back to Proton uses the Proton prefix.
- `GAMEID` is `umu-0` for every game: umu's per-game fixes (protonfixes by store id) are not
  looked up.
- The Steam Linux Runtime is fetched by umu itself on the first Proton run, without progress in
  openSU; umu keeps it current on later runs.
