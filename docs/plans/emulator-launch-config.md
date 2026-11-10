# Standalone emulators set up for each launch

Requested 2026-11-10: "we should have this [automatic Cemu pad config] for other non-libretro
emulators like Switch ones or also maybe for all of them", and "Eden ... launches asking for update,
which can't be dismissed without a mouse or keyboard ... we should still be able to update
emulators. We can make our own updater".

## What it does

- Every standalone emulator openSU starts takes input from the virtual pads the game is given, with
  no setup in the emulator: the player's own profile bound to those pads when one exists, a default
  pad profile written when none does.
- No emulator stops on a dialog a pad cannot close at start: its own update check, first-run
  wizard or "new version" prompt is off for openSU's launches.
- The player's own files come back byte-identical when the emulator ends; a crash's leftovers are put
  back at the next launch. Starting the emulator outside openSU behaves as before.
- Updates come from openSU: Settings › Emulators checks and installs each emulator's official
  build (`library/emulator_release`, `app/emulator_downloads`), for every emulator that publishes
  one.

## Order

1. One owner for launch-time config: what `library/cemu_pads` does for Cemu (set for the run, put
   back after, leftovers recovered), extended to a per-emulator rule, with Cemu its first user.
   Built: `library/emulator_launch_config` (the rules), `library/launch_files` (set, put back,
   recovered; a file the player lacked made and removed).
2. Eden and its family (Citron, Sudachi): the Eden update prompt reproduced from boot and turned off for the
   launch; `qt-config.ini`'s `player_N_*` SDL bindings retargeted to the virtual pads' GUID and
   port, and a default profile when the player has none. Built (`library/yuzu_config`): the
   update check off; SDL bindings retargeted to the virtual pad by the GUID Eden keys pads by (the
   name's CRC zeroed, `SDLDriver`'s `GetGUID`) and port N for player N; a Pro Controller for
   player 1, or a connected player, without SDL bindings. Proved with the player's Eden nightly on
   a copy of their config, Kirby Star Allies read-only (`scratch/eden-launch/after2-nightly`):
   before, no press reached the title; after, the pad's presses took it from "Press A" to file
   select, and `qt-config.ini` came back byte-identical. The update prompt the player reported was
   not reproduced: the nightly, v0.2.1 and v0.2.0, with the player's config and with a fresh one
   (`check_for_updates=true`), showed none from boot; upstream builds `ENABLE_UPDATE_CHECKER` off
   by default. A finding, not a fix: the player's Eden has player 1 on the keyboard and their pad
   on player 2, and no virtual pad reached Eden, so any prompt in the game took the keyboard.
3. Each other standalone emulator, each with a reproduction of whether a virtual pad reaches the game today
   and what its config binds by (SDL index, name or GUID): Ryujinx, Dolphin, PCSX2, RPCS3,
   DuckStation, PPSSPP, Azahar, melonDS, xemu, Xenia Canary, shadPS4, mGBA. One emulator per change.

   Survey, 2026-11-10, before any per-emulator rule. Runs: openSU `--hidden` in a headless
   Gamescope launching the game for a hidden guest, the emulator on a scratch copy of the player's
   config, eight guest presses (Start, A, A, Down, A, Start, A, B) with a frame before and after
   each, and the input nodes the emulator held (`scratch/pad-survey/`). "Source" is read from
   upstream. In every run the virtual pad was `event20`, SDL GUID `0300cece5e0400008e02000010010000`.

   | Emulator | Binds by | Input today | Start-up prompt | Fix needed |
   |---|---|---|---|---|
   | Ryujinx 1.3.3 | GUID: `Config.json` `input_config[].id` = `<k>-<GUID>`, the SDL GUID as a .NET Guid with the name CRC zeroed, `k` the lowest index free among connected pads of that GUID (`SDL2GamepadDriver.GenerateGamepadId` in 1.3.3, `SDL3GamepadDriver` later) | None, reproduced: Chocobo's Mystery Dungeon kept "Press Any Button" through every press, Ryujinx opened no input node. The player's `Config.json` is version 71 with `GamepadSDL3` and audio `SDL3`, from their own Ryubing master build (2b929c55, no longer installed); 1.3.3 reads up to 70 | `update_checker_type` (`Off`, `PromptAtStartup`, `CheckInBackground`; before version 65 `check_updates_on_start`); the player's is `PromptAtStartup` | Built (`library/ryujinx_config`): each gamepad entry's `id` made `<N-1>-00000003-045e-0000-8e02-000010010000` for the Nth virtual pad, mappings kept; a keyboard player, or player 1 without an entry, a Pro Controller in Ryujinx's default pad layout; `update_checker_type: Off`, `check_updates_on_start: false`; `GamepadSDL3` written `GamepadSDL2`, a version 71 file migrated down to 70 (its audio `SDL3` back to `SDL2`), a newer one left the player's. Proved with 1.3.3 on a copy of the player's config (`scratch/ryu-azahar/runs/ryujinx2`): presses took the title from "Press Any Button" to Select Saved Game and Select Play Mode, no update prompt, `Config.json` sha256 `998773f3…` before and after, the real file untouched |
   | Dolphin 2503a (Fedora) | Name and index: `GCPadNew.ini` `[GCPadN]` and `WiimoteNew.ini` `[WiimoteN]` `Device = SDL/<n>/<SDL name>` or `evdev/<n>/<evdev name>`, `n` the lowest id free among devices of that source and name (`ControllerInterface::AddDevice`). Per game: `GameSettings/<ID>.ini` `[Controls]` `PadProfileN`/`WiimoteProfileN` load `Profiles/GCPad\|Wiimote/<name>.ini` from the config folder, whose `[Profile]` `Device` binds the same way (`InputConfig::LoadConfig`); the player has none | None, reproduced: DKC Returns kept "Press any button on the Wii Remote" through Start, A, A, Down, A until its attract demo; Dolphin held `event20`, but the player's GCPad and Wiimote slots are `XInput2/0/Virtual core pointer` (keyboard and mouse) | Analytics permission (`[Analytics] PermissionAsked`), shown even with `-b` in builds with analytics (upstream Flatpak; Fedora builds it off); the updater is skipped in batch mode | Retarget only the `Device` lines, in the two ini files and in any per-game profile a game's `[Controls]` names, to `SDL/<N-1>/Xbox 360 Controller`, mappings kept; a slot bound to keyboard and mouse gets the default instead, since its mappings name keys. The default for a player with no pad profile is a standard GameCube pad only; Wii defaults are step 4. `PermissionAsked = True` |
   | PCSX2 (player's AppImage) | Index: `inis/PCSX2.ini` `[PadN]` `<button> = SDL-<n>/<input>`, `n` the SDL player index or the first free one (`SDLInputSource::OpenDevice`) | None, reproduced: Crash Twinsanity's attract reel ignored Start and A; the log has "Controller SDL-0 connected" (the virtual pad is SDL-0), but `[Pad1]` is bound to `Keyboard/*` | Setup wizard when `[UI] SetupWizardIncomplete = true` (a fresh config), even with `-batch`; the update check is skipped when a game boots | A pad bound to the keyboard or to no SDL device gets default `SDL-<N-1>` bindings; `SDL-<n>` bindings already reach the virtual pads; `SetupWizardIncomplete = false` |
   | RPCS3 (player's AppImage) | Name: `input_configs/global/Default.yml` `Player N Input:` `Handler: SDL`, `Device: "<SDL name> <k>"`, `k` counting same-name pads from 1 (`sdl_pad_handler`) | None, reproduced: Time Crisis 4's "(X) OK" autosave notice stayed through every press; RPCS3 opened no input node (player 1 is `Keyboard`, player 2 `Null`) | `GuiConfigs/CurrentSettings.ini` `[main_window] infoBoxEnabledWelcome` (welcome dialog; the player's is false); `[Meta] checkUpdateStart` runs only in the main window, which `--no-gui` skips | Player N: `Handler: SDL`, `Device: "Xbox 360 Controller N"` (the virtual pad's SDL3 name, as PCSX2 logs it); an SDL or evdev player's mappings kept with only `Device` retargeted, a keyboard player given the default pad mapping |
   | DuckStation (latest, downloaded) | Index, as PCSX2: `settings.ini` `[PadN]` `<button> = SDL-<n>/<input>` | Not reproduced: Gamescope streamed, but no picture reached openSU's stream, DuckStation opened no input node and wrote nothing to its config. From source the player's `[Pad1]` is `SDL-0/*`, which is the virtual pad | Update check at every start (`MainWindow::startupUpdateCheck`, `[AutoUpdater] CheckAtStartup`, default true), its dialog over a fullscreen boot; setup wizard when `[Main] SetupWizardIncomplete` | `CheckAtStartup = false`; default `SDL-<N-1>` bindings for a pad without SDL ones; the blank start is investigated first |
   | PPSSPP | No device: every SDL pad is `DEVICE_ID_PAD_0` (`SDLJoystick.cpp`), so any pad drives the mapping in `controls.ini` | Source only: no PSP game in the player's folders | None modal: `[General] CheckForNewVersion` adds a dismissable bar on the main menu, which a game passed on the command line skips | None found; confirm with a PSP game |
   | Azahar 2126.2 | GUID and port: `qt-config.ini` `profiles\N\<button>="engine:sdl,guid:<GUID>,port:<k>,..."`, `k` the kth pad of that GUID (`maptype` `guid+port` by default, `SDLButtonFactory`) | None, reproduced: OoT3D's "PRESS START" ignored Start and A until its attract demo; Azahar held `event20`, but the player's profile is keyboard only | `[UI] check_for_update_on_start` (the player's is true): a dialog when a newer release exists (`OnEmulatorUpdateAvailable`) | SDL entries' `guid` made the virtual pad's and `port` N-1; a keyboard-only profile gets a default SDL profile; `check_for_update_on_start=false`. `qt-config.ini` holds absolute `nand_directory` and `sdmc_directory`, which stay the player's |
   | melonDS 1.1 (downloaded) | Index: `melonDS.toml` `[Instance<i>]` `JoystickID` (SDL joystick index, 0 when out of range), raw SDL joystick codes in `[Instance<i>.Joystick]` | None, reproduced: Dawn of Sorrow's intro played through Start and A; the fresh `melonDS.toml` has `JoystickID = 0` and every Joystick bind -1; melonDS held `event20` | None | Default `[Instance0.Joystick]` codes for the virtual pad's buttons, `JoystickID = 0`, when the player has none |
   | xemu | GUID: `xemu.toml` `[input.bindings]` `portN = '<GUID>'`, and with `[input] auto_bind` (default true) a pad that matches no port takes the first port with nothing connected (`xemu-input.c`); same-GUID pads take any port listing that GUID | Source only: the player's `xemu.toml` names no MCPX, BIOS or HDD, so no game boots; it has no bindings, so the virtual pad would auto-bind to port 1 | `[general] show_welcome` (first-boot window; the player's is false); the update check is Windows-only | `portN` set to the virtual pad's GUID for players 2-4 when a port is `keyboard`; `show_welcome = false` |
   | Xenia Canary (player's AppImage) | No config: the SDL driver gives a pad its SDL player index or the first free user slot (`SDLInputDriver::OnControllerDeviceAdded`) | Not reproduced: Marvel Ultimate Alliance ran 2.5 min, but no picture reached openSU's stream. Xenia's log has the virtual pad "Added at index 0" | None found | None expected for binding |
   | shadPS4 (launcher, core `dd18a5f`) | Order: `TryOpenSDLControllers` gives connected pads slots 0-3 in SDL order, each slot only when `users.json` has a user with that `player_index`; buttons by name in `input_config/*.ini`; `Input.default_controller_id` is unused | Source only: the run was stopped by low memory (16 GB machine, Bloodborne) before the presses. Its log has "Gamepad registered for slot 0" for the virtual pad | Launcher `qt_ui.ini` `[general_settings]` `checkForUpdates`, `checkCompatibilityAtStartup`, `showChangeLog` (the player's are all false) | A `users.json` user per extra player; launcher checks off |
   | mGBA 0.10.5 | GUID then order: the player's preferred device (`gba.input.SDLB` `device<i>`, an SDL GUID) else the first unclaimed pad; buttons from `[gba.input.SDLB]` and a profile keyed by the SDL joystick name (`InputController::profileForType`) | Buttons none, reproduced: Kirby's DEMO ran through Start and A until it ended on its own; mGBA held `event20`. Without a profile only the stick and hat are bound (`mSDLInitBindingsGBA`); its built-in profiles match names like "Microsoft X-Box 360 pad", and the virtual pad's SDL name is "Xbox 360 Controller" | None: `updateAutoCheck` is off by default | A `[gba.input-profile.Xbox 360 Controller]` profile with the face, shoulder, Start and Select buttons when the player has none |

   Renaming the virtual pad (`gamepad/virtual_pad`) to xpad's "Microsoft X-Box 360 pad" was
   considered as one owner-level fix and fixes none of these. SDL2 and SDL3 name 045e:028e
   "Xbox 360 Controller" from their own table before reading the device name
   (`SDL_CreateJoystickName`, `GuessControllerName`), so mGBA's profiles still miss and SDL-name
   keys (Dolphin, RPCS3) do not move. No emulator here auto-binds a recognised pad by name.
   The rename would only change the GUID's name CRC and Dolphin's `evdev/` name. The Cemu rule
   takes that CRC from `gamepad/sdl_guid`, as Azahar and xemu rules would; Eden and Ryujinx zero it. A keyboard-bound player 1 (PCSX2, RPCS3,
   Azahar, Dolphin) needs the per-emulator rule in any case.
4. Wii per-game controls. A Wii game's controls depend on how it holds the remote: sideways
   remote, remote plus Nunchuk, or Classic Controller, with the pointer (IR) and shake or motion.
   A per-game table, keyed by game ID, picks the emulated Wii Remote's extension and orientation.
   It also says where the pointer goes on a pad (the right stick works best) and where shake and
   motion go. A game not listed gets a generic remote plus Nunchuk. The pick is written as a
   Dolphin per-game input profile (`GameSettings/<ID>.ini` `[Controls] WiimoteProfileN` and its
   `Profiles/Wiimote/<name>.ini`) for the launch and put back after, like the other overrides; a
   player's own per-game profile is retargeted (step 3), never replaced. Examples: Donkey Kong
   Country Returns (`SF8E01`; sideways remote or remote plus Nunchuk, roll is a shake, so shake on a
   face or shoulder button) and Super Mario Galaxy (remote plus Nunchuk, pointer on the right stick,
   spin is a shake on a button). Proof game: the player's Donkey Kong Country Returns (USA) (Rev 1).
5. Release sources for every emulator with an official Linux build, so openSU's own Update covers
   them, and an update openSU finds shown on Downloads. Built: Ryujinx (Ryubing's Forgejo,
   `git.ryujinx.app`, no digest), Citron (`citron-neo/CI`'s `nightly-linux` release, the only Linux
   build its site links), Xenia Canary (`xenia-canary/xenia-canary`), and shadPS4 and melonDS,
   whose AppImage ships alone in a zip (`"member"`; shadPS4's is its SDL build, so a downloaded
   AppImage takes the launcher its name matches). The check runs at start and daily while openSU
   downloaded something; each update it finds is a Downloads row with an Update button. No source:
   Dolphin (Linux builds are its Flatpak repositories only), sudachi (no reachable official
   release: its site serves no working HTTPS and its GitHub repositories are gone). Shown with
   Eden: v0.2.0 planted, the start-up check offered v0.2.1 on Downloads and in Settings, Update
   installed it beside the old one and switched, and the next launch ran it
   (`scratch/emu-update/proof/`).

## Libretro versions

Where a libretro core of the same emulator exists (Dolphin, LRPS2, PPSSPP, melonDS DS, SwanStation,
Citra), offering it as a built-in core sidesteps step 3 for that console: the runner owns its input.
That is `docs/issues/more-libretro-cores.md`'s list. No core exists for Switch, Wii U, PS3, PS4 or
Xbox 360; a libretro port of Eden or Cemu is its own project (`docs/ideas.md`), not part of this plan.
