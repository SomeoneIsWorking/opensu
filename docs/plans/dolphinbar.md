# Mayflash DolphinBar and real Wii Remotes

Requested 2026-11-11: the player owns a Mayflash DolphinBar and wants openSU to support it. Wii
games should keep the player's real Wii Remotes, since a real remote beats an emulated layout, and
keep the emulated layout only for players who have none.

## What it does

- A Wii game played on Dolphin uses the real Wii Remotes whenever they are here: a DolphinBar in its
  Wii Remote mode, or a remote connected over Bluetooth. A player without one plays the game's
  emulated remote (`library/wii_controls`) on their virtual pad.
- The Devices screen says what Wii games will play: "DolphinBar (Wii Remote mode)", or that the bar
  should be switched to mode 4.
- Outside Wii games, a Wii Remote can be an ordinary pad, held sideways or with a Nunchuk (step 4).
- The bar's IR pointer moves openSU's own pointer, so the pointer-first Wii-inspired theme can be
  used with a remote (step 5).

## What the hardware does (from the sources)

- The bar has four modes, chosen with its button; its LEDs show the mode: 1 keyboard and mouse
  (default), 2 keyboard and mouse for games, 3 gamepad with the IR pointer as a mouse, 4 Wii
  Remote mode for Dolphin (Mayflash's manual, as quoted by manualshive.com and manuals.online).
- In mode 4 it is one USB device `057e:0306` ("HJZ Mayflash Wiimote PC Adapter") with four HID
  interfaces, one per slot, each bound by `hid-generic` with its own hidraw node. All four are
  there whether or not a remote is synced; only a write tells an empty slot from a full one (a
  status request fails with EPIPE). Dolphin's `IOhidapi.cpp` says so and does that probe.
- In mode 3 it is `0079:1803` (the kernel's `hid-mf.c`, "Mayflash Wireless Sensor DolphinBar",
  multi-input). The ids of modes 1 and 2 are not documented anywhere found; the player's check
  (below) reads them.
- Linux's `hid-wiimote` binds `057e:0306` and `057e:0330` only on Bluetooth (`HID_BLUETOOTH_DEVICE`
  in `hid-wiimote-core.c`), so it never takes a DolphinBar slot. A Bluetooth remote is bus `0005`
  and gets evdev devices "Nintendo Wii Remote" (keys only: the d-pad as KEY_UP..., BTN_A, BTN_B,
  BTN_1, BTN_2, KEY_NEXT, KEY_PREVIOUS, BTN_MODE; no ABS_X/ABS_Y), "... Accelerometer", "... IR"
  (ABS_HAT0X to ABS_HAT3Y) and one per extension ("... Nunchuk", "... Classic Controller").
- Dolphin finds real remotes through hidapi on hidraw: by name prefix "Nintendo RVL-CNT" or by
  `057e:0306`/`0330`, then a status request. It needs read and write access to the hidraw node:
  Dolphin's udev rules (`SUBSYSTEM=="hidraw*", ATTRS{idVendor}=="057e", ATTRS{idProduct}=="0306"`,
  `TAG+="uaccess"`). Its source says the kernel's `wiimote` driver conflicts with that access for a
  Bluetooth remote. Real remotes fill the slots whose `[WiimoteN] Source` is 2 (none 0, emulated
  1; slot 1 defaults to emulated, the others to none), in slot order (`ProcessWiimotePool`).
  `[Core] WiimoteContinuousScanning` keeps looking after start; `[BluetoothPassthrough] Enabled`
  takes a Bluetooth adapter over instead, which leaves hidapi remotes unused.
- SDL 3.2+ has a HIDAPI Wii driver for `057e:0306`/`0330`, off by default on every platform
  (`SDL_HINT_JOYSTICK_HIDAPI_WII`) because "it doesn't work with the dolphinbar". It maps a bare
  remote upright (A and B east and south, 1 and 2 west and north), a Nunchuk's C and Z to the left
  shoulder and trigger and its stick to the left stick, has no sideways mapping, gives the
  accelerometers and Motion Plus as sensors and does not read the IR camera.

## Order

1. Detection. Built (`device/wii_remotes`): `findWiiRemotes` reads `/sys/bus/hid/devices/*/uevent`
   (`HID_ID`, `HID_PHYS`): a bar in Wii Remote mode is the USB `057e:0306` interfaces grouped by
   their USB device, one in mode 3 is `0079:1803`, a Bluetooth remote is bus `0005` with
   `057e:0306` or `0330`. Whether Dolphin can reach the bar is `access(R_OK|W_OK)` on each slot's
   hidraw node, which honours the uaccess ACL; nothing is opened. Real remotes are present when a
   bar is in mode 4 or a Bluetooth remote is connected; since an empty bar slot looks like a full
   one, mode 4 itself is the player's word that they play with remotes. A hidden run reads only
   `<data>/hidden-hid/{devices,dev}` (`HidPlaces::of`), as it reads only `hidden-pads`. Tested
   with fake trees (`wii_remotes` test).
2. The Dolphin rule. Built (`library/dolphin_config`, `LaunchSetup::realWiiRemotes` from the
   shell's launch): for a Wii game with real remotes here, each slot the player set to a real
   remote stays one; a slot openSU made emulated is real again; slot 1 is made real when no slot
   is; `WiimoteContinuousScanning` is turned on so a remote switched on after the start is found.
   Emulated slots keep the game's layout on their virtual pad. Without real remotes, slot 1 and
   every slot with a remote are emulated as before, and a real remote's slot gets the note
   `openSU/Real Remote = True` in its `[WiimoteN]` section, which Dolphin does not read and keeps
   when it saves (`InputConfig::SaveConfig` loads the file and sets its own keys). Since launch
   rules write the config for good, that note is what gives the slot back. A noted slot the player
   has emptied since loses the note and stays empty. Passthrough stays off for every Wii game.
   Tested (`dolphin_config` test) on scratch configs; an after-run on Dolphin with a fake bar is
   step 2's proof below.
3. The Devices line. Built (`app/devices_controller`): the Controllers tab adds "DolphinBar (Wii
   Remote mode)" ("Wii games play the remotes synced to it", or "Dolphin cannot open it: install
   Dolphin's udev rules"), "DolphinBar" with "Switch it to mode 4 (Wii Remote) to play Wii games on
   real Wii Remotes" for a bar in mode 3, and the Bluetooth remotes counted. Tested
   (`devices_controller` test).
4. Wii Remotes as pads outside Wii games. Not built. Two paths:
   - Bluetooth remotes already have evdev devices from `hid-wiimote`. `gamepad/evdev_device`'s
     `isPad` refuses them (no ABS_X/ABS_Y), so openSU neither reads nor grabs them, and Dolphin's
     hidapi path is untouched. A `PadTranslator` layout for "Nintendo Wii Remote" (sideways: d-pad
     turned a quarter, 1 and 2 as south and east, minus and plus as select and start, Home as
     Guide) and for the remote with its Nunchuk device (stick as the left stick, C and Z as
     shoulder and trigger) would make them pads; the two evdev devices of one remote share its
     `uniq` and are one pad. While a Wii game runs on real remotes, openSU must not grab them,
     since Dolphin needs the remote itself; the claim for a Wii game leaves `hid-wiimote` devices
     free.
   - DolphinBar slots have no evdev device. Reading them means hidraw (SDL's HIDAPI Wii driver, or
     openSU's own), which would hold the slot Dolphin needs. SDL's driver is off by default because
     it fails on the bar, maps no sideways remote and needs a write per slot to find the full ones.
     Recommendation: pads from Bluetooth remotes through evdev first; bar slots only with openSU
     letting go of every hidraw node before Dolphin starts, after a real-bar trial of SDL's driver
     with `SDL_HINT_JOYSTICK_HIDAPI_WII=1`.
5. The IR pointer in openSU's own UI. Not built. In mode 3 the bar is a mouse already, so its
   pointer reaches the shell through SDL's mouse events into `PointerFrame`
   (`app/shell_window`), with nothing to build; only the player's check confirms it. In mode 4 the
   pointer needs the remote's IR camera turned on over hidraw (output reports 0x13/0x1a and the IR
   sensitivity block), which only works while Dolphin is not running, and the dots turned into a
   point (the bar's LEDs are the sensor bar). A Bluetooth remote's "Nintendo Wii Remote IR" evdev
   device gives the dots without any writes; turning its two brightest dots into a point and
   feeding it to `ShellWindow::movePointer`, with A as the primary button, is the first pointer
   path to build. It goes through the shell's existing pointer input, so the Wii-inspired theme
   needs nothing of its own.
6. A settings row. Not built. Settings › Controls › "Wii Remotes in Wii games": Real when here
   (default) or Emulated always, for a player who keeps the bar in mode 4 but plays with a pad.
   It is read into the launch as `LaunchSetup::realWiiRemotes` and nothing else changes.

## Proof

- Steps 1 to 3: focused tests (`wii_remotes`, `dolphin_config`, `devices_controller`).
- Step 2 in Dolphin, proved on scratch copies of the player's config with DKC Returns, a hidden host
  in headless Gamescope and a guest pressing Start and A (`scratch/dolphinbar/runs/`):
  `wii-absent`, the player's real-remote slot 1, logged "Wiimote1 is an emulated remote set for
  Donkey Kong Country Returns" and wrote `Source = 1`, `openSU/Real Remote = True` and the
  sideways layout on `SDL/0/Xbox 360 Controller`; the presses reached the game ("Controls set to
  horizontal position"). `wii-present`, that noted slot with a fake mode-4 bar under
  `<data>/hidden-hid`, logged "Wiimote1 is a real remote again" and wrote `Source = 2`, the note
  gone, `WiimoteContinuousScanning=True`; the game stayed on "Press any button on the Wii Remote",
  since the pad no longer plays slot 1. The player's real files were untouched in both. A real
  remote reaching the game needs the player.

## Player's check with the real bar

1. Plug the bar in, press its mode button until LED 4 is lit (Wii Remote mode), then run
   `grep -H . /sys/bus/hid/devices/*/uevent | grep -i -e 057e -e 0079` and send the output.
2. Run `ls -l /dev/hidraw*` and `getfacl /dev/hidraw*` with the bar in mode 4: each slot's node
   should give your user `rw`. If not, Dolphin needs its udev rules (Fedora's `dolphin-emu`
   package ships them; the Flatpak cannot).
3. Open openSU's Devices › Controllers: it should say "DolphinBar (Wii Remote mode)" and "Wii games
   play the remotes synced to it".
4. Sync a remote to the bar (press the bar's sync button, then the remote's red sync button), start
   a Wii game from openSU and play it on the remote. Then check `~/.config/dolphin-emu/WiimoteNew.ini`
   (or the Flatpak's): `[Wiimote1] Source = 2`.
5. Unplug the bar, start the same game with a pad: it should play the emulated remote, and
   `[Wiimote1]` should read `Source = 1` with `openSU/Real Remote = True`. Plug the bar back in and
   start it again: `Source = 2`, the note gone.
6. Press the mode button to modes 1, 2 and 3 in turn and send step 1's output for each. In mode 3,
   Devices should tell you to switch to mode 4; point the remote at the screen and say whether
   openSU's pointer follows it.
