# Integrations are opt-in, Steam has modes

Status: partial. Integrations, the first-run chooser and all four Steam modes are built; Isolated
and its controller option are shown working with the player's real Steam (As built); what is left
is listed under Left.

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
| Isolated | Managed in a bwrap sandbox: the player's own Steam, HOME and login, inside openSU's Gamescope with its windows hidden once logged on; it cannot reach the desktop's display, session bus (only the Secret Service) or runtime sockets, and sees only openSU's virtual pads unless the player chooses every controller. |

### Isolated: measured 2026-10-21

Run with the player's real Steam client and login, tools in `scratch/isolated-steam/tools/`,
logs in `scratch/isolated-steam/logs/`, frames in `scratch/isolated-steam/shots/`. No system
setting was changed and no OS protection (SELinux, userns limits, seccomp, ptrace, polkit) refused
any step; `user.max_user_namespaces = 63182`, `kernel.unprivileged_userns_clone` does not exist on
this kernel, `bwrap` 0.12.0 is installed.

What worked:

- Display: openSU's Gamescope fork headless outside any sandbox
  (`gamescope --backend headless -W 1280 -H 720 -w 1280 -h 720 -r 60 -- sleep infinity`, runtime
  dir of its own, `PIPEWIRE_RUNTIME_DIR` the player's). It starts Xwayland `:1` and a PipeWire
  node `gamescope` (Video/Source). Steam with `DISPLAY=:1` drew its library there
  (`shots/steam-library-2.png`); nothing appeared on the desktop.
- Login: the player's own client and data logged on by itself both runs, no Steam Guard prompt
  (`connection_log.txt` `[Logged On ...] processing complete` 25 s after start). DevTools
  (`-cef-enable-debugging`, 127.0.0.1:8080, SharedJSContext) answered: logged in, 403 games,
  installed list (`logs/q2-library.json`).
- Game: Hamsteria Demo (4440600, native, no cloud) launched with `SteamClient.Apps.RunGame`, ran in
  Steam Linux Runtime 2.0 (soldier) pressure-vessel, drew into the headless Gamescope
  (`shots/game-hamsteria.png`), closed with `SteamClient.Apps.TerminateApp`. It sticks at 34 % on
  a missing texture in its own pack (its Godot log), not a sandbox fault.
- PipeWire capture (`tools/pw_capture.py`, GStreamer `pipewiresrc path=<node>`): BGRx 1280x720,
  58–60 fps steady (`logs/q3a-pw-*.log`, `shots/pw-*.png`). X commit to frame on the consumer side:
  min 6.7, median 19.9, max 24.0 ms over 20 flips (`tools/flip.c`, `logs/q3a-latency.txt`).
  Remote Play was not needed.
- Shutdown: `steam -shutdown` run in the same sandbox ended the client in under a minute.

Sandbox (`tools/sandbox.py`, bwrap): `/` read-only, `/proc` the host's bound read-write, `/dev`
minimal plus `/dev/dri` and only the given `/dev/input/eventN`, `/dev/shm` and `/tmp` private with
just Gamescope's `X1` socket bound in, `/run/user/1000` an empty tmpfs (no Wayland, PipeWire,
Pulse or session D-Bus), `/run/udev` hidden, HOME a tmpfs with `~/.local/share/Steam`, `~/.steam`
and `/mnt/Boy/SteamLibrary` bound read-write, network shared, `--clearenv`.

- Nesting: Steam's webhelper (sniper) and the game (soldier) pressure-vessel containers ran inside.
  The one failure was this sandbox's own mount, not an OS protection: with `/proc` bound read-only,
  pressure-vessel's bwrap failed `bwrap: setting up uid map: Read-only file system`
  (`logs/q1-pv-nested.log`); `/proc` bound read-write, or `--unshare-pid --proc /proc`, fixes it.
- Input (`tools/vpad.c`, uinput Xbox 360 ids): inside, `/dev/input` held only the exposed
  `event20`; the second virtual pad `event21` was `No such file or directory`; a press on
  `event20` arrived inside (`logs/q4-sandbox-read.log`). Steam and the game held only `event20`
  (`logs/q4-input-fds.log`); Steam's log: `Xbox 360 Controller`, opened for index 0. No `/dev/uinput`
  or `/dev/hidraw*` inside, so Steam Input cannot create its own virtual pad and hidraw-only pads
  are invisible. Nodes are bound at start, so pads must exist before Steam starts (fixed slots).
  Host programs see the virtual pads too: Godot editors of other agents opened them.
- Audio off: Steam logged `PulseAudio connect failed (used only for Mic Volume Control) with error:
  Access denied` and ran on; the game's Godot fell back to its dummy driver after ALSA
  `ERR_CANT_OPEN`.
- Display reach: the desktop Xwayland's abstract socket is reachable (shared network namespace) but
  refused `Authorization required` because its cookie stays hidden. The system bus is reachable.
  Steam autolaunched a private session D-Bus through X (`dbus-launch --autolaunch`), which outlives
  the sandbox and has to be ended with it.

Blockers found:

- A private HOME breaks saves. With HOME on a tmpfs, Steam Cloud downloaded the cloud saves of
  Baba Is You, Celeste, Upload Labs and others into the throwaway HOME at start; a game's progress
  there would vanish or race the real files through the cloud. This contradicts the player's
  decision anyway (same data): Isolated must give Steam the player's real HOME.
- Gamescope fork crash: SIGSEGV after a PipeWire consumer disconnected while a frame was being
  copied (`logs/gamescope-segv.txt`: `radv_handle_image_transition` from `vulkan_screenshot` on
  the compositor thread, `destroy_buffer` on the PipeWire thread). Steam lost its display and died
  uncleanly with it. The fork needs the PipeWire buffer lifetime guarded before openSU consumes the
  stream.
- Steam's first start after a while spends ~40 s verifying and updating its own client files.

Alternative measured: `systemd-run --user -p PrivateDevices=yes -p ProtectHome=tmpfs -p
BindPaths=... -p PrivateUsers=yes` runs unprivileged but `PrivateDevices` also drops `/dev/dri`
and offers no per-node input list (`logs/q1-systemd-run.log`).

The design that followed from these measurements, a second headless Gamescope whose PipeWire node
openSU draws, was dropped for hiding Steam inside openSU's own Gamescope (below): no capture, no
added latency, and the fork's PipeWire teardown crash no longer matters for Steam. The bwrap layer
was kept for the devices and the desktop, with the real HOME. The Docker idea is not needed.

### Which mode applies

- The player on Normal: "if someone has Steam already running and they don't want isolated Steam,
  they should just be able to use it". Normal attaches to a Steam already running on the desktop
  and never starts a second one.
- In session mode there is no desktop Steam to attach to, so Normal is not offered there; Managed
  (as SteamOS runs Steam) is the default and Isolated stays a choice.

- Isolated uses the player's own Steam install and data, not a copy ("isolated Steam should reuse
  login", "Steam should use the same thing so it shouldn't be THAT isolated"): same login, same
  library folders, same client. What it isolates is the screen and the devices: Steam's windows on
  a display of their own and only openSU's virtual pads. One Steam client per data dir, so the
  desktop Steam cannot run beside it, as in Managed.

## As built

- `settings::Integrations::inForce` (`src/settings/integrations.hpp`) is the one rule: no saved
  choices is everything off; Steam's mode falls back to the default when the saved one is not
  offered (Normal on a desktop, Managed in `--session`, where Managed and Isolated are offered).
  The Steam pads choice (`settings::SteamPads`) applies only in Isolated and defaults to
  openSU's virtual pads. `app::IntegrationsInForce` holds it
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
- Managed and Isolated hide Steam's windows in openSU's Gamescope once Steam is logged on:
  `IntegrationSwitch::service`, every frame, sets the fork's root `GAMESCOPE_HIDE_STEAM_CLIENT`
  through `session::SteamWindowHiding` to 1 for Managed or Isolated and `SteamState::Ready`, and
  to 0 otherwise (starting, not logged on, gone, stopped, another mode), so a login or client
  update window stays visible. The fork then leaves windows of `steam`/`steamwebhelper`
  processes, app id 769 and `STEAM_BIGPICTURE` out of focus, the composite, the overlay pick and
  `GAMESCOPE_FOCUSABLE_WINDOWS`; the games Steam starts are focused and drawn as usual. Proof:
  `scratch/gamescope-hide/` (`tools/drive.py`, headless fork, an `xmessage` copy named `steam` in
  red and one named `game` in blue): unset or 0, the Steam window is focused and drawn and both are
  focusable, as with the old pin (`logs/baseline.txt`); 1, the game is focused and drawn and only it
  is focusable (`logs/fork.txt`, `shots/fork-*.png`). A hidden openSU set to Managed, with no
  Steam started, sets 0 (`logs/opensu.txt`). The shipping `IntegrationSwitch`, `steam::Client` and
  `SteamWindowHiding` with a stand-in `steam` in its own scope (`tools/ready_driver.cpp`, scratch
  HOME, DevTools port 1; a hidden openSU never starts Steam): Stopped and Initializing 0, Ready 1,
  Steam turned off 0, Managed again 0 until Ready, 1, the client gone 0 (`logs/ready.txt`,
  `shots/ready-*.png`: red Steam window shown at 0, blue game at 1).
- Isolated mode (`steam::Client::startOwn` with `Mode::Isolated`): the Managed start (orphan
  check, refusal while a desktop Steam runs, `systemd-run --user --scope` unit
  `<session>-steam.scope`, `-silent -cef-enable-debugging`) with the command wrapped in bwrap
  (`steam::sandboxArguments`, `src/steam/sandbox.cpp`). Window hiding is Managed's: `guardsSteam`
  is true for Managed and Isolated, so the atom goes to 1 once Steam is logged on, the shell holds
  the pads and Big Picture is closed. The sandbox, unprivileged, no pid or net namespace:
  - `--bind / /`: the real HOME, Steam install, library folders and login, read-write.
  - `--dev /dev` plus the GPU nodes (`/dev/dri`, `/dev/nvidia*`), no `/dev/uinput` or hidraw.
  - `/dev/input`: openSU's virtual pad nodes only, with `/run/udev` a tmpfs (virtual), or the
    host's `/dev/input` and udev db (every controller).
  - `/tmp` private with only openSU's Gamescope X socket; `DISPLAY` that Xwayland,
    `WAYLAND_DISPLAY`, `XAUTHORITY`, `SESSION_MANAGER` unset, so the desktop's display is
    unreachable (its abstract socket refuses without the cookie).
  - The runtime dir a 0700 tmpfs with only `pipewire-0`, `pulse/native` and `gamescope-0` bound in
    (game audio, openSU's Gamescope); `/dev/shm` private; `--new-session`.
  - Session bus: a socket of `xdg-dbus-proxy <desktop bus> --filter
    --talk=org.freedesktop.secrets` (`steam::SecretBus`, unit `<session>-steam-bus.scope`). Steam
    reads its login from the Secret Service: with no bus it never logged on (240 s); with none set
    it autolaunched a private bus that started a second `ksecretd` on the player's wallet, portals
    and gvfsd that outlived Steam. Through the proxy it logs on in ~12 s and nothing lingers.
  Missing `bwrap` or `xdg-dbus-proxy` refuses the start with the install command per platform.
- Isolated controllers: bwrap binds nodes once at start, so pads cannot appear later.
  `gamepad::Pads::keepVirtualPads(4)` keeps four uinput Xbox 360 pads alive for Steam's run and
  `IntegrationSwitch::moveSteam` hands their nodes to the sandbox; under the Game claim real pads
  drive the kept pads first, and a kept pad is rested when its real pad leaves or the claim ends.
  Choices in Integrations › Steam, "Steam sees":
  - openSU's virtual pads (default): "Your controllers stay hidden from Steam". Steam and its
    games can never see the real pads or Guide, even when the shell lets the pads go.
  - Every controller: "openSU grabs them while it is in front". Steam sees every input node; the
    shell's grab still keeps them from Steam while openSU holds them. No pads are kept.
  Left out: Steam Input on a virtual pad openSU hands it needs `/dev/uinput` and nodes made after
  start, which a static bind cannot expose without the real pads, and turning Steam Input on for
  that pad is a Steam setting; Steam owning the real pads contradicts "I'd rather Steam not grab
  the device". Neither was measured.
- `--steam` makes a `--hidden` run start Steam in its mode (a hidden run otherwise starts none).
- Steam imports screenshots taken through Gamescope (`gamescope_control take_screenshot`) into
  its own screenshot library for the running app and removes the originals.
- Proof, 2026-10-21, `scratch/isolated-steam/tools/isolated_proof.py run {virtual|all} 4440600`:
  openSU `--hidden --steam` in the headless Gamescope fork, the player's real HOME and Steam,
  scratch XDG dirs, audio off (`PULSE_SERVER` unreachable, `SDL_AUDIODRIVER=dummy`); desktop Steam
  not running before either run. PipeWire capture was not used (Gamescope logged
  `pw_context_connect failed`; games drew straight into openSU's Gamescope).
  - Logon with the existing login, no prompt: `[steam] logged on` 11.5 s (virtual) and 14.6 s
    (all) after start; the atom read 0 while initializing and 1 at logon
    (`logs/proof-virtual.txt`, `logs/proof-all.txt`).
  - Launch through openSU (A on the tile, A on Play): Hamsteria Demo focused, the only focusable
    window (`GAMESCOPE_FOCUSED_WINDOW` = `GAMESCOPE_FOCUSABLE_APPS`); frames
    `shots/proof-{virtual,all}-ready.jpg` (Steam hidden: black), `-game.jpg` (the game's loading
    screen), `-after-game.jpg`.
  - Virtual: the sandbox's `/dev/input` held `event20`–`event23` (the kept pads), no hidraw,
    uinput or udev db; Steam, its `winedevice.exe` and the game held only those four.
  - Every controller: the sandbox's `/dev/input` matched the host's (`event0`–`event19`, mice, the
    Logitech receiver; no gamepad was connected) with the udev db; Steam held no input node.
  - Shutdown with openSU: Steam ended in ~3 s, both scopes gone, the kept pads destroyed.
  - UI: `shots/proof-ui-integrations-{virtual,all}.png`.
- A game openSU launched still holds the pads in every mode, so Guide chords stay over Normal-mode
  games.
- Proof: `scratch/integrations/drive.py` (`fresh`, `normal`) drives a hidden run with a scratch
  HOME/XDG, a fake Steam root through `OPENSU_STEAM_ROOTS`, and stand-in `steam`, `legendary`,
  `gogdl`; tests `integrations`, `integration_flow`, `integration_chooser`, `settings_controller`,
  `client`, `handoff`, `catalog_loader`, `focus_owner`.

## Left

- Isolated, unproven: "Every controller" with a real pad connected (the grab and release against
  Steam); real pad input forwarded into a game through the kept pads; game audio through the
  bound PipeWire and Pulse sockets; Isolated in `--session`; NVIDIA nodes.
- Isolated with every controller: Steam sees the real pads while the shell has let them go
  (another window in front), by design of that choice.
- Managed hides every Steam window once Steam is logged on, so a dialog Steam shows after logon
  (EULA, cloud conflict, a launch option or error) is invisible. openSU should show Steam's windows
  while Steam shows a dialog the player must answer. Not run against the real client.
- Normal-mode installs and the download queue go through Steam's CEF DevTools, which a desktop
  Steam only serves when started with `-cef-enable-debugging`; without it those fail with an error.
- Normal mode has not been run against a real desktop Steam.
- The player wants the pad held over Normal-mode games: "Sure if it works, I'd rather Steam not grab the device"; kept.

### Hiding Steam inside openSU's own Gamescope (preferred over a second Gamescope)

The player: "Can't we make Gamescope support hidden things?". The fork already tells Steam's
windows apart: `steamcompmgr.cpp` marks a window `bIsSteamPid` / `bIsSteamWebHelperPid` from its
process and `window_is_steam` for Big Picture (app id 769 or `STEAM_BIGPICTURE`). A fork commit can
keep those windows out of focus and out of the composite whenever openSU asks (a root window atom
openSU sets, so it can still show a Steam dialog it wants). Games Steam starts carry their own app
ids and are shown as usual, drawn straight by Gamescope with no capture: no second Gamescope, no
PipeWire stream, no added latency, and the PipeWire crash above stops mattering for Steam. Device
isolation stays separate (the optional bwrap layer, or Steam Input off and the pads held).

### Steam's view of the controller: an option, several ways to try

The player: "we can try a few things to get the controller to work the way we want and we can make
it an option". Candidates, each measured on the player's pad before it becomes a choice:

1. openSU holds the real pads (today): Steam and its games see only openSU's virtual pads.
2. The bwrap layer: Steam's process only has openSU's virtual pad nodes (measured above: works;
   Steam Input cannot add its own pad without `/dev/uinput`).
3. Steam Input on a virtual pad openSU hands it: Steam remaps that pad for its games while openSU
   keeps the real one and its Guide chords.
4. Steam sees the real pads (no isolation), for players who want Steam to have them.

Built for Isolated as "Steam sees" (As built): 2 is the default, 1 is "Every controller"; 3 and 4
are left out with the reasons there.
