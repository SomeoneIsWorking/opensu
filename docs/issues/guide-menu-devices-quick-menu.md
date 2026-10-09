# Guide menu, Devices page and quick menu

Status: open, queued after the settings screen.

Requested (user): a devices menu with Bluetooth and controllers, modelled closer to Steam than
iiSU: a left menu on Guide, and possibly a quick menu on Guide + A.

Plan:

- Guide opens a left menu, as Steam's main menu does: Home, Library, each store, Devices,
  Settings, and Power (sleep, restart, shut down, quit to desktop). It reuses the existing Guide
  overlay (`session/gamescope_overlay.cpp`), so it also opens over a running game.
- Devices is one page with these tabs:
  - Bluetooth: BlueZ over D-Bus; scan, pair, connect, forget.
  - Controllers: each pad with battery, button test and player order, plus chord remapping.
  - Audio output.
  - Display.
- Guide + A opens a quick menu on the right for in-place changes: volume and output device,
  brightness, Wi-Fi and Bluetooth toggles, controller batteries, and close game. It shares the
  volume owner with Settings.

Order: left menu and Devices first, then the quick menu, then checking both over a game.
