# Integrations are opt-in, Steam has modes

Status: partial. Integrations, the first-run chooser and the Off, Normal and Managed Steam modes
are built; Isolated is not (it needs the sandbox measurements below first).

The player's words: "I think we shouldn't enable all integrations initially, we can add an option
to integrate Steam for example ... maybe they just wanna use it as libretro", and for Steam: keep
the isolation, but also offer a fully isolated Steam ("you can put it in docker and just connect to
it via remote connection ... possibly not via using docker") and "an option to just use Steam
normally, no isolation".

## Integrations

- Built in and always on: the ROM library and the libretro runner.
- Opt-in, each a switch in Settings › Integrations: Steam, Epic (legendary), GOG (gogdl),
  standalone emulators (downloads and launcher discovery), Lua plugins.
- An integration that is off starts nothing, reads nothing (no Steam client, no `legendary`, no
  library scan of that store), and its section, tiles, dock badge and Settings pages are absent.
- First run: a chooser lists the integrations, all off, with what each one does and needs. The
  chooser shows whenever the settings file has no integration choices, so an existing install sees
  it once too; no silent migration.

## Steam modes

| Mode | What openSU does with Steam |
| --- | --- |
| Off | Nothing; no Steam games listed. |
| Normal | Uses the player's own Steam as it is: starts it on the desktop if it is not running, launches games through it, never holds the pad against it, never closes its windows. |
| Managed (today's behaviour) | Starts Steam in the background inside openSU's Gamescope, ends it with openSU, holds the pad in the shell so Steam never sees Guide, closes Big Picture. |
| Isolated | A separate Steam that cannot see the desktop, the real pads or the player's files: its own home, its own headless display, only openSU's virtual pads; openSU shows its games. |

### Isolated without Docker: to measure first

1. Sandbox: `bwrap` (unprivileged user namespaces) or `systemd-run --user` with a private home and
   device list; the Steam data dir separate from the player's, library folders shared or not.
2. Display: a headless Gamescope for that Steam. Showing its games to the player, either
   a. Steam Remote Play from the isolated Steam to a client in openSU's session (Steam Link), or
   b. the headless Gamescope's PipeWire stream drawn by openSU, with input through the virtual pads
      inside the sandbox; no network involved.
3. Whether Steam runs at all in the sandbox (its runtime is itself a container: pressure-vessel
   inside bwrap), and Steam Input and the overlay there.

### Which mode applies

- The player on Normal: "if someone has Steam already running and they don't want isolated Steam,
  they should just be able to use it". Normal attaches to a Steam already running on the desktop
  and never starts a second one.
- In session mode there is no desktop Steam to attach to, so Normal is not offered there; Managed
  (as SteamOS runs Steam) is the default and Isolated stays a choice. Session mode does not need
  Isolated.

- Isolated reuses the player's Steam login ("isolated Steam should reuse login"): the sandbox gets
  the login state from the player's Steam data, not a separate account. Library folders are shared
  too unless the player says otherwise.

## As built

- `settings::Integrations::inForce` (`src/settings/integrations.hpp`) is the one rule: no saved
  choices is everything off; Steam's mode falls back to the default when the saved one is not
  offered (Normal on a desktop, Managed alone in `--session`). `app::IntegrationsInForce` holds it
  for the run and saves a new choice; `app::IntegrationSwitch` applies a change live (catalog
  `replace`, plugins host, standalone emulator discovery and downloads, pad and Big Picture guards,
  Steam stopped and started in its mode on a worker).
- Chooser: `app::FirstRunChooser` over `ui::IntegrationChooser`; opens when `settings.json` has no
  `integrations` key or a damaged one. The control channel reports it as `firstRunOpen`.
- Settings › Integrations: `app::IntegrationSettings`. Steam libraries, the Library page's Steam
  rows, Wine (Epic or GOG), Plugins and each store's install folder show only with their
  integration on.
- Normal mode (`steam::Client::startNormal`): attaches to a desktop Steam found by
  `~/.steam/steam.pid`, a live pid and `/proc/<pid>/comm == steam`; if none runs it starts `steam`
  with `systemd-run --user --collect`, outside openSU's scope, never ended by openSU. A hidden run
  logs the decision and starts nothing. The shell does not hold the pad against it or close Big
  Picture, and the handoff does not wait for a Steam window in openSU's Gamescope.
- A game openSU launched still holds the pads in every mode, so Guide chords stay over Normal-mode
  games.
- Proof: `scratch/integrations/drive.py` (`fresh`, `normal`) drives a hidden run with a scratch
  HOME/XDG, a fake Steam root through `OPENSU_STEAM_ROOTS`, and stand-in `steam`, `legendary`,
  `gogdl`; tests `integrations`, `integration_flow`, `integration_chooser`, `settings_controller`,
  `client`, `handoff`, `catalog_loader`, `focus_owner`.

## Left

- Isolated mode: the sandbox measurements above, then a design.
- Normal-mode installs and the download queue go through Steam's CEF DevTools, which a desktop
  Steam only serves when started with `-cef-enable-debugging`; without it those fail with an error.
- Normal mode has not been run against a real desktop Steam.
- Open question for the player: keep Guide chords over Normal-mode games (today: kept)?
