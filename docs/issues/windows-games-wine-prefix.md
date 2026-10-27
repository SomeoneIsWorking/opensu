# Windows games share one Wine prefix; Steam's compatibility handling is a setting

Status: partial. The shared prefix, the per-game own prefix and the runner setting are built;
Steam's "Use <compatibility tool>" is not built (below). Project state S038.

Requested: "By default, use a common wineprefix for all games, no need to create separate
prefixes unless specified, Steam can be left alone on how it handles it or we can also send
proton path to Steam, can be configurable how Steam is handled".

## Built

- One owner, `library/wine.*`: `prepare` adds `--wine <runner> --wine-prefix <prefix>` to the
  launch of a GOG or Epic game whose install is a Windows build (`Game::windowsInstall`, set by
  `gog.cpp` from the record's platform and by `epic.cpp` from legendary's `platform`) and makes the
  prefix folder. Any other game, Steam's included, is left as it is. gogdl gets
  `gogdl launch <path> <id> --platform windows --wine ... --wine-prefix ...`; legendary gets
  `legendary launch <app> --wine ... --wine-prefix ...`.
- Prefix: `<data dir>/wine-prefix` (`$XDG_DATA_HOME/opensu/wine-prefix`) for every game, or
  `<data dir>/wine-prefixes/<store>-<id>` for a game set to its own (`settings.json`
  `ownWinePrefix`, a list of game ids). The details page of a Windows install has a "Wine prefix"
  button reading Shared or Own; A switches it. Going back to Shared keeps the own folder.
- Runner: Automatic by default, openSU's GE-Proton kept up to date, else Steam's newest Proton,
  else what Lutris uses, else system Wine, without waiting; a Proton build runs through umu-run.
  Settings › Wine picks another runner, pins a game to one and lists every runtime with its games:
  `docs/issues/ge-proton-default.md`. Steam's and Lutris's folders are only read.
- A launch is refused with a toast when no runner is found, or the picked runner is gone.
- Settings › Wine also shows the shared prefix's path, how many games have their own, and "Steam
  games: Left to Steam".
- `GogPaths::prefixes` (`<data>/gog-prefixes/<id>`) is gone without a migration: no GOG Windows
  game had been installed on a real account.
- Uninstall never removes a prefix, shared or own (`uninstall-games.md`).

Tests: `tests/library/wine_test.cpp` (discovery against a fake PATH and fake Steam folders,
prefixes, arguments, refusals), `gog_test`, `epic_test` (`windowsInstall`), `settings_test`
(round trip), `game_details_test` and `details_page_flow_test` (the row, kept on disk),
`settings_controller_test` (the Wine category, runner stepping, Steam's folders unchanged, a launch
prepared with the picked runner), `uninstall_job_test` (both prefixes kept).

## Not built: "Use <compatibility tool>" for Steam games

Leaving Steam's own choice is the only behaviour, shown as an info row, so no setting exists yet.
Writing Steam's `CompatToolMapping` (in `<Steam root>/config/config.vdf`, or through the client)
is not built, because what the running client does with it is unmeasured. Before building it,
measure on a test Steam install, never the player's:

1. Whether `SteamClient.Apps.SpecifyCompatTool(appId, toolName)` (or what Steam's own
   Properties › Compatibility page calls) exists in the SharedJSContext and takes effect for the
   next launch without a restart.
2. Whether an edit of `config.vdf` while Steam runs is overwritten when Steam exits, and whether
   one made while Steam is stopped is read on the next start.
3. The tool name Steam expects for a `compatibilitytools.d` tool (its `compatibilitytool.vdf`
   key) and for Steam's own Proton builds (`proton_9`, `proton_experimental`).

The setting would then be global with a per-game override, written only by the path proven, and
tested against a fake Steam root and the fake DevTools Steam.
