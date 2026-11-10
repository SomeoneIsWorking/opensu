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
   | Dolphin 2503a (Fedora) | Name and index: `GCPadNew.ini` `[GCPadN]` and `WiimoteNew.ini` `[WiimoteN]` `Device = SDL/<n>/<SDL name>` or `evdev/<n>/<evdev name>`, `n` the lowest id free among devices of that source and name (`ControllerInterface::AddDevice`). Per game: `GameSettings/<ID>.ini` `[Controls]` `PadProfileN`/`WiimoteProfileN` load `Profiles/GCPad\|Wiimote/<name>.ini` from the config folder, whose `[Profile]` `Device` binds the same way (`InputConfig::LoadConfig`); the player has none | None, reproduced: DKC Returns kept "Press any button on the Wii Remote" through Start, A, A, Down, A until its attract demo; Dolphin held `event20`, but the player's GCPad and Wiimote slots are `XInput2/0/Virtual core pointer` (keyboard and mouse) | Analytics permission (`[Analytics] PermissionAsked`), shown even with `-b` in builds with analytics (upstream Flatpak; Fedora builds it off); the updater is skipped in batch mode; the NKit warning before any NKit disc, even with `-b` (`MainWindow::StartGame`, `NKitWarningDialog`, skipped by `[Interface] SkipNKitWarning`), reproduced: Luigi's Mansion (nkit) never booted | Built (`library/dolphin_config`): `Dolphin.ini` `[Analytics] PermissionAsked` and `[Interface] SkipNKitWarning` true; each `[GCPadN]`, `[WiimoteN]` and per-game `[Profile]` on an `SDL/` device made `SDL/<N-1>/Xbox 360 Controller`, mappings kept; a GameCube slot or profile on anything else (keyboard and mouse, evdev) given a standard GameCube pad there (A south, B west, X east, Y north, Z right shoulder); a Wii Remote not on SDL left as it is (step 4). Per-game profiles are found from every `GameSettings/*.ini` in Dolphin's data folder (catalog `"data"`), a name or a folder of them, the slot naming one first giving its pad. Proved with the player's Dolphin on copies of their config (`scratch/dolphin-launch/runs/`): `dolphin-gc`, Luigi's Mansion with pad 1 on the keyboard, the A presses took it from the intro to file select, New Game and the mansion's first floor; `dolphin-wii`, DKC Returns with an emulated Wii Remote whose per-game profile names another SDL pad, the presses took it from the title through "Controls set to horizontal position" to Select Game and a new game. `GCPadNew.ini`, `Dolphin.ini`, `WiimoteNew.ini`, the profile and `SF8E01.ini` came back byte-identical; only Dolphin's own `TimePlayed.ini` and `btdinf.bak` changed |
   | PCSX2 (player's AppImage) | Index: `inis/PCSX2.ini` `[PadN]` `<button> = SDL-<n>/<input>`, `n` the SDL player index or the first free one (`SDLInputSource::OpenDevice`) | None, reproduced: Crash Twinsanity's attract reel ignored Start and A; the log has "Controller SDL-0 connected" (the virtual pad is SDL-0), but `[Pad1]` is bound to `Keyboard/*` | Setup wizard when `[UI] SetupWizardIncomplete = true` (a fresh config), even with `-batch`; the update check is skipped when a game boots | Built (`library/pcsx2_config`): `[UI] SetupWizardIncomplete = false`, and `SettingsVersion = 1` in a file without it, which PCSX2 would reset to defaults with the wizard on; `[PadN]`'s SDL bindings made `SDL-<N-1>`, inputs kept; a DualShock 2 without SDL bindings (pad 1, or one typed `DualShock2`) given PCSX2's own SDL layout on `SDL-<N-1>`, Analog left off Guide; `inputprofiles/*.ini` bound the same way. Proved on the player's AppImage with a copy of their config (`scratch/launch-rules/runs/pcsx2`): Start and A took Twinsanity from its attract reel through the intro to New Game and the memory card prompt; `PCSX2.ini` came back byte-identical |
   | RPCS3 (player's AppImage) | Name: `input_configs/global/Default.yml` `Player N Input:` `Handler: SDL`, `Device: "<SDL name> <k>"`, `k` counting same-name pads from 1 (`sdl_pad_handler`) | None, reproduced: Time Crisis 4's "(X) OK" autosave notice stayed through every press; RPCS3 opened no input node (player 1 is `Keyboard`, player 2 `Null`) | `GuiConfigs/CurrentSettings.ini` `[main_window] infoBoxEnabledWelcome` (welcome dialog; the player's is false); `[Meta] checkUpdateStart` runs only in the main window, which `--no-gui` skips | Built (`library/rpcs3_config`): every profile under `input_configs/<folder>/` (and `global/Default.yml`, RPCS3's fallback, made when the player has none) binds player N to `Device: Xbox 360 Controller N`: an SDL player keeps its mappings; player 1, or a player on any other handler but `Null` (keyboard, evdev, which enumerates `/dev/input` in directory order, or a DualShock over hidraw), gets `sdl_pad_handler::init_config`'s layout with PS on Guide; `[main_window] infoBoxEnabledWelcome=false`, since the welcome dialog opens even with `--no-gui`. Proved on the player's AppImage with a copy of their config (`scratch/launch-rules/runs/rpcs3`): RPCS3 held the virtual pad, and each A (Cross) closed one of Time Crisis 4's notices, autosave, save data and install, to its Guncon3 screen; `Default.yml`, `CurrentSettings.ini` and `active_input_configurations.yml` came back byte-identical |
   | DuckStation (latest, downloaded) | Index, as PCSX2: `settings.ini` `[PadN]` `<button> = SDL-<n>/<input>` | Not reproduced: Gamescope streamed, but no picture reached openSU's stream, DuckStation opened no input node and wrote nothing to its config. From source the player's `[Pad1]` is `SDL-0/*`, which is the virtual pad | Update check at every start (`MainWindow::startupUpdateCheck`, `[AutoUpdater] CheckAtStartup`, default true), its dialog over a fullscreen boot; setup wizard when `[Main] SetupWizardIncomplete` | `CheckAtStartup = false`; default `SDL-<N-1>` bindings for a pad without SDL ones; the blank start is investigated first |
   | PPSSPP | No device: every SDL pad is `DEVICE_ID_PAD_0` (`SDLJoystick.cpp`), so any pad drives the mapping in `controls.ini` | Source only: no PSP game in the player's folders | None modal: `[General] CheckForNewVersion` adds a dismissable bar on the main menu, which a game passed on the command line skips | None found; confirm with a PSP game |
   | Azahar 2126.2 | GUID and port: `qt-config.ini` `[Controls]` `profiles\N\<button>="engine:sdl,guid:<GUID>,port:<k>,..."`, SDL's full GUID with the name CRC kept, `k` the kth pad of that GUID (`maptype` `guid+port` by default, `SDLButtonFactory`); the active profile is `profile` (0-based) | None, reproduced: OoT3D's "PRESS START" ignored Start and A until its attract demo; Azahar held `event20`, but the player's profile is keyboard only | `[Miscellaneous] check_for_update_on_start` (the player's is true): a dialog when a newer release exists (`OnEmulatorUpdateAvailable`) | Built (`library/azahar_config`): the active profile's SDL bindings retargeted to the virtual pad's GUID and port 0, keys kept; a profile without SDL bindings given the standard layout through SDL's game controller API, positional as Azahar's own xpad automap (3DS A east, B south); `check_for_update_on_start=false`; `nand_directory` and `sdmc_directory` stay the player's. Proved with Azahar 2126.2 on a copy of the player's config (`scratch/ryu-azahar/runs/azahar`): Start took "PRESS START" to file select, the south button (3DS B) back to the title and the east one (3DS A) to file select again; the scratch `qt-config.ini` sha256 `87d32db0…` before and after, the real one `09bdca4b…` untouched |
   | melonDS 1.1 (downloaded) | Index: `melonDS.toml` `[Instance<i>]` `JoystickID` (SDL joystick index, 0 when out of range), raw SDL joystick codes in `[Instance<i>.Joystick]` | None, reproduced: Dawn of Sorrow's intro played through Start and A; the fresh `melonDS.toml` has `JoystickID = 0` and every Joystick bind -1; melonDS held `event20` | None | Built (`library/melonds_config`): `[Instance0] JoystickID = 0`; the player's `[Instance0.Joystick]` codes kept, and when every DS button is -1 the virtual pad's raw SDL codes in the DS's positional layout (A b1, B b0, X b3, Y b2, L b4, R b5, Select b6, Start b7, the D-pad on hat 0 or the left stick); the file made for the run when the player has none. Proved with the downloaded 1.1 (`scratch/launch-rules/runs/melonds`): Start skipped Dawn of Sorrow's intro to its title, A opened the title menu at Game Start, and the next A, DS B, backed out of it; `melonDS.toml` was absent before and after. The player has no melonDS settings; with the file melonDS 1.1 writes on its first start, every button -1, as the before-state (`runs/melonds-seeded`) the same presses did the same and the file came back byte-identical |
   | xemu | GUID: `xemu.toml` `[input.bindings]` `portN = '<GUID>'`, and with `[input] auto_bind` (default true) a pad that matches no port takes the first port with nothing connected (`xemu-input.c`); same-GUID pads take any port listing that GUID | Source only: the player's `xemu.toml` names no MCPX, BIOS or HDD, so no game boots; it has no bindings, so the virtual pad would auto-bind to port 1 | `[general] show_welcome` (first-boot window; the player's is false); the update check is Windows-only | `portN` set to the virtual pad's GUID for players 2-4 when a port is `keyboard`; `show_welcome = false` |
   | Xenia Canary (player's AppImage) | No config: the SDL driver gives a pad its SDL player index or the first free user slot (`SDLInputDriver::OnControllerDeviceAdded`) | Not reproduced: Marvel Ultimate Alliance ran 2.5 min, but no picture reached openSU's stream. Xenia's log has the virtual pad "Added at index 0" | None found | None expected for binding |
   | shadPS4 (launcher, core `dd18a5f`) | Order: `TryOpenSDLControllers` gives connected pads slots 0-3 in SDL order, each slot only when `users.json` has a user with that `player_index`; buttons by name in `input_config/*.ini`; `Input.default_controller_id` is unused | Source only: the run was stopped by low memory (16 GB machine, Bloodborne) before the presses. Its log has "Gamepad registered for slot 0" for the virtual pad | Launcher `qt_ui.ini` `[general_settings]` `checkForUpdates`, `checkCompatibilityAtStartup`, `showChangeLog` (the player's are all false) | A `users.json` user per extra player; launcher checks off |
   | mGBA 0.10.5 | GUID then order: the player's preferred device (`gba.input.SDLB` `device<i>`, an SDL GUID) else the first unclaimed pad; buttons from `[gba.input.SDLB]` and a profile keyed by the SDL joystick name (`InputController::profileForType`) | Buttons none, reproduced: Kirby's DEMO ran through Start and A until it ended on its own; mGBA held `event20`. Without a profile only the stick and hat are bound (`mSDLInitBindingsGBA`); its built-in profiles match names like "Microsoft X-Box 360 pad", and the virtual pad's SDL name is "Xbox 360 Controller" | None: `updateAutoCheck` is off by default | Built (`library/mgba_config`): `config.ini` `[gba.input-profile.Xbox 360 Controller]` given each key the player's lacks from mGBA's own xpad profile (A on button 1, B on 0, Select 6, Start 7, L 4, R 5), with the d-pad on hat 0 and the left stick on axes 0 and 1. Proved with the downloaded 0.10.5 on a copy of the player's config (`scratch/dolphin-launch/runs/mgba`): Start left Kirby's DEMO for the title, Down moved to the language row and the east button (GBA A) opened the language menu; `config.ini` came back byte-identical, only mGBA's own `qt.ini` changed |

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
   Built (`library/wii_controls`, `assets/wii-controls.json`; `library/dump_info` reads the disc's
   game id from its header, plain, RVZ/WIA or WBFS): named layouts (nunchuk, the default; sideways;
   classic) set the extension, `Options/Sideways|Upright Wiimote`, the pointer on the right stick
   (relative, recentred by its click), shake on a button and the rest of the remote's inputs; games
   are keyed by id or its first three characters. For a Wii game the Dolphin rule makes Wii Remote
   slot 1, and any real remote's slot, emulated (`Source = 1`) on the virtual pad, since a pad
   player is not holding a remote; a slot not on SDL gets the layout in `WiimoteNew.ini` for the
   run rather than a generated per-game profile, so a player's own per-game profile, which Dolphin
   reads instead of the slot, still wins and is only retargeted; Bluetooth passthrough is off.
   Listed: DKC Returns (`SF8`, sideways: 2 on south, 1 on west, shake on east or the right
   shoulder, the D-pad on the D-pad and left stick), Super Mario Galaxy (`RMG`, nunchuk) and
   Xenoblade Chronicles (`SX4`, classic), the last two unproven. Proved with the player's Dolphin on
   copies of their config, Wii Remote 1 a real remote (`scratch/dolphin-launch/runs/`, Dolphin's
   input display on in the copy): `wii-table`, presses took DKC Returns from its title to a new
   game, a held shake broke Donkey Kong out of his hut, and the D-pad walked him through 1-1 (pad
   right read as the remote's down, the sideways turn); `wii-nunchuk`, the nunchuk layout for the
   same game, A+B took its Nunchuk-style title, both shakes pounded the hut and the left stick read
   as the Nunchuk's stick, but the run's shakes did not break the hut open, so Donkey Kong walking
   by the stick is not seen. The player's folder holds no other Wii game. Real files byte-identical
   in every run; only Dolphin's `TimePlayed.ini` and `btdinf.bak` changed in the copies.
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
