# Integrations are opt-in, Steam has modes

Status: open, design.

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

## Open questions for the player

- Isolated Steam: its own login and its own installs, or share the existing library folders?
- Normal mode: keep openSU's Guide chords over Steam games (needs the pad held during a game)?
