# Windows games share one Wine prefix; Steam's compatibility handling is a setting

Status: open.

Requested: "By default, use a common wineprefix for all games, no need to create separate
prefixes unless specified, Steam can be left alone on how it handles it or we can also send
proton path to Steam, can be configurable how Steam is handled".

## Current state

- GOG: `library/gog.cpp:Provider` launches a Windows install with
  `gogdl launch ... --wine <setup_.wine> --wine-prefix <data>/gog-prefixes/<id>`, one prefix per
  game (`library/gog_installs`, `GogPaths::prefixes`). The Wine binary is `GogSetup::wine`,
  default `wine` on PATH.
- Epic: `library/epic.cpp` launches `legendary launch <app>` with no Wine arguments, so legendary's
  own config decides the runner and prefix.
- Steam: openSU never touches compatibility tools; Steam runs each game with whatever the user
  picked in Steam.

## Gap and design

1. One shared prefix by default, `<data>/opensu/wine-prefix`, used by every Windows game openSU
   launches outside Steam (GOG and Epic: `legendary launch <app> --wine <runner> --wine-prefix
   <prefix>`). A per-game "own prefix" setting gives that game `<data>/opensu/wine-prefixes/
   <store>-<id>`. The prefix choice has one owner (a resolver beside the other per-game settings),
   used by both store providers; `GogPaths::prefixes` goes.
2. The Wine runner is a setting (default system `wine`); a Proton build as the runner is an open
   item: Proton outside Steam needs `STEAM_COMPAT_DATA_PATH` and its own launcher (umu-launcher is
   the common one), not a bare `--wine` path.
3. Steam: a setting with two choices, "Leave to Steam" (default) and "Use <compatibility tool>",
   global with a per-game override. The second writes Steam's `CompatToolMapping` for the app;
   measure whether the running client accepts it through `SteamClient` or only through
   `config.vdf` while Steam is stopped.
4. Uninstall (`uninstall-games.md`): the shared prefix is never removed by an uninstall; an own
   prefix is kept by default, since it may hold saves.

No GOG Windows game has been installed on a real account yet (project state, GOG section), so the
per-game prefixes are dropped without a migration.
