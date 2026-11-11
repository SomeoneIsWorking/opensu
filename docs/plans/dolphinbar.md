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
4. Wii Remotes as pads. Built (`gamepad/wii_remote_layout`, `gamepad/wii_slot`,
   `gamepad/evdev_pads`). A Bluetooth remote: `hid-wiimote`'s "Nintendo Wii Remote" device, refused
   before by `isPad` (no ABS_X/ABS_Y), is a pad when its ids are `057e:0306`/`0330`. Alone it is
   held sideways (the d-pad turned a quarter, 2 as A, 1 as B, B and A as Y and X); with its
   "Nintendo Wii Remote Nunchuk" device it is held upright (A and B as A and B, 1 and 2 as X and Y,
   the stick as the left stick, its Y turned to grow downwards, C and Z as L1 and L2, the
   accelerometer dropped); with its "Nintendo Wii Remote Classic Controller" device only the
   Classic is read, its buttons by place as `assets/wii-controls.json`'s emulated Classic maps them
   (B, A, Y, X as A, B, X, Y; ZL and ZR as L1 and R1; L and R fully in as L2 and R2, their travel
   dropped; both sticks, Y turned). Plus, minus and Home are Start, Select and Guide. The parts are
   paired by the device the kernel made them for (`/sys/class/input/<node>/device/device`, which
   `hid-wiimote` shares between them); a part seen before its remote waits for it. A DolphinBar in
   mode 4: the shell (not the runner, which reads only what the shell gives it) opens each slot's
   hidraw node (`device::WiiRemotes::barSlots`, found again as hidraw nodes appear) and speaks the
   remote's reports there (`WiiSlot`): a status request every second, which the bar refuses for an
   empty slot; the slot's player light; report 0x30, or 0x32 with an extension set up unencrypted
   and named by its id (Nunchuk or Classic Controller). What it reads comes out as `hid-wiimote`'s
   events, so both kinds are one layout. A node that is not a character device is refused, so the
   earlier fake bar's plain files open nothing. While a Wii game plays real remotes
   (`LaunchSetup::realWiiRemotes`), the shell's hold is `WiiRemoteHold::ToGame`: every remote's
   evdev devices and every bar slot are closed before the game starts (closing the camera and
   extension devices also stops the kernel changing the remote's report mode), and a remote's part
   that appears meanwhile is told from sysfs and never opened; when the hold ends they are opened
   again and are pads. Tested: `wii_remote_layout` (the mapping through the translator),
   `wii_slot` (a slot against a stand-in remote on a packet socket pair: empty slot, light, modes,
   Nunchuk and Classic setup and bytes) and `pads` (a uinput remote read sideways with its camera
   and its Classic, let go for a Wii game with none of its nodes held and taken back after; a bar
   slot on a pseudo-terminal under a private bar tree, its remote a pad, its Classic read, let go and
   taken back). The real parent pairing and the real bar's protocol are shown only on the real
   hardware (the player's check).
5. The IR pointer in openSU's own UI. Built for Bluetooth remotes: the remote's "Nintendo Wii
   Remote IR" device (the kernel turns the camera on while it is open) is read as part of its pad
   (`gamepad::WiiPointer`): the middle of the first two dots, mirrored across (as wiiuse does), is
   the camera's aim; the range set (`WiiPointerRange`) says where the screen is in those aims, and
   an aim past its edges, or no dot, is off the screen. The dots there when the camera opens are
   read at once. They come as `Event::Kind::Pointer`, and `app/pointing_pads` moves the shell's
   pointer there (`ShellWindow::movePointer`, the same path as the mouse). While the shell holds the
   pads, the remote's own A pressed as it points is the pointer's button
   (`Event::Kind::PointerButton`), let go wherever it then points; the window keeps a posted
   pointer's place between its events until the remote points away, when the pointer leaves, or
   SDL's own mouse comes over the window. Fitting (`app/wii_remote_settings`, Settings › Controls ›
   Wii Remote pointer): the pads read the camera's whole view, the player points at the screen's
   top-left corner and presses A, then the bottom-right; the two aims are the screen's edges
   (`WiiPointerRange::fromCorners`), which also fixes an axis read the other way, and where the
   camera's middle falls says whether the sensor bar is above or below the screen (`barPlace`). The
   range is saved (`settings.json` `wiiPointer`) and set on the pads at start. No roll correction.
   In mode 3 the DolphinBar is a mouse already and reaches the shell through SDL's mouse events. In
   mode 4 the slot's camera is not turned on, so a bar's remote does not point. Tested:
   `wii_remote_layout` (dots to aim, the range from corners, the bar's place) and `pointing_pads`.
6. A settings row. Built (`app/wii_remote_settings`): Settings › Controls › "Wii Remotes in Wii
   games": Real when here (default) or Emulated always, for a player who keeps the bar in mode 4
   but plays with a pad (`settings.json` `realWiiRemotes`). The launch reads it with the remotes'
   presence into `LaunchSetup::realWiiRemotes`, which also picks the hold's `WiiRemoteHold`.

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
- Step 4's let-go in Dolphin (`scratch/dolphinbar2/runs/wii-bluetooth/`): a fake Bluetooth remote
  (its bus `0005` HID device under `<data>/hidden-hid`, hid-wiimote's remote and camera as uinput
  devices in `hidden-pads`) was a pad and pointed with its camera; on Play openSU logged "Nintendo
  Wii Remote (...) is left to the game" before Dolphin started, and while DKC Returns ran openSU
  held only `/dev/uinput` (Dolphin's own evdev backend opens every input node, the fakes too);
  slot 1 stayed `Source = 2`. The player's real files were untouched.
- Steps 4 to 6 in a hidden openSU (`scratch/dolphinbar2/proof2/`): a mode-4 bar slot on a
  pseudo-terminal under `<data>/hidden-hid`, a stand-in remote answering on it, and a uinput
  Bluetooth remote with its Classic Controller and camera. The slot was asked for its status,
  lit player light 1 (`0x10`) and set report 0x30; its remote's d-pad down moved the focus right
  (sideways); with a Classic plugged in it set up 0x55/0x00, read the id, logged "read as its
  Classic Controller", switched to 0x32, and the Classic's d-pad left moved the focus back. The
  Bluetooth Classic's right moved the focus. Settings › Controls shows the Wii Remotes rows (frame
  `6-settings-wii-pointer.png`); fitting with the camera aimed at 0.2, 0.6 and 0.8, 0.9 logged
  "fitted: across 0.200 to 0.800, down 0.600 to 0.900" and the row read "Fitted, sensor bar above
  the screen" (`7-pointer-fitted.png`); aiming at 0.5, 0.75 put the pointer on the middle of the
  screen (`8-pointer-middle.png`).

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
7. With the bar unplugged, pair a remote over Bluetooth (Devices › Bluetooth, press 1 and 2
   together). It should be listed under Controllers as "Nintendo Wii Remote"; held sideways, 2 is
   A and the d-pad moves the focus. Plug in the Nunchuk: the stick moves the focus, the d-pad is
   no longer turned. Point the remote at the screen with a sensor bar on: the pointer should
   follow it (say if up and down are swapped), and A should press what it points at.
8. With the bar in mode 4 and a remote synced, outside a game: the remote's player light should
   light and, held sideways, its d-pad should move the focus. Plug a Classic Controller into it:
   its d-pad and buttons should move and press as a pad's do. Then start a Wii game: the remote
   should play it (openSU lets go of it), and after the game it moves the focus again.
9. Settings › Controls › Wii Remote pointer, with a Bluetooth remote and a sensor bar: press A,
   point at the top-left corner and press A, then the bottom-right and press A. The row should
   say whether the sensor bar is above or below the screen, as it is; the pointer should then
   reach every edge. Set "Wii Remotes in Wii games" to Emulated always and start a Wii game with
   the bar plugged in: it should play on the pad, `[Wiimote1] Source = 1`.
