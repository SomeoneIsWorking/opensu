# Session mode on Bazzite and SteamOS

Status: open, not reproduced (read from a ROG Ally on bazzite-deck:stable, 2026-11-13; nothing run).

Expected: on a Bazzite Deck or SteamOS machine, the player can enter openSU's login session from
Gaming Mode or Desktop Mode, and leave it back to where they came from. If openSU fails to start,
they don't get stuck in a loop.

## What lines up

- `/usr/local` is `/var/usrlocal` (writable), `/etc` is writable, and pkexec, sudo, visudo and
  restorecon are present, so `install-session.sh` can place its three files.
- SDDM's `SessionDir` includes `/usr/local/share/wayland-sessions`.
- Autologin is on: `zz-bazzite-autologin.conf` sets `User=`, and `zz-holo-autologin.conf` sets
  `Session=`. `zz-opensu-session.conf` sorts after both, so while it exists openSU is the
  autologin session. `restore` removes it, and Bazzite's file decides again.

## What does not

1. **Switching needs Plasma.** `Switch to session mode` ends the session with KDE's
   `org.kde.Shutdown.logout`, which Gaming Mode (gamescope-session) doesn't have, so it refuses
   there. Bazzite and SteamOS switch sessions through steamos-manager:
   `steamosctl set-default-desktop-session <entry>` and `switch-to-desktop-mode`, called by
   `/usr/libexec/os-session-select` and needing no root.
2. **A failed start repeats.** `/usr/lib/sddm/sddm.conf.d/holo.conf` sets `[Autologin] Relogin=true`.
   While openSU is selected, any exit other than Switch to desktop (a crash, or Gamescope failing on
   the DRM backend) logs straight back into openSU. The way out is another VT and
   `sudo /usr/local/libexec/opensu-session-select restore`.
3. **Steam's "Switch to Desktop" can land in openSU.** steamos-manager writes `zz-holo-autologin.conf`,
   which our drop-in overrides while it exists.

## Proper fix (to plan)

Session selection gets one owner with two backends chosen by what the system provides:

- steamos-manager, where `steamosctl` exists: no sudoers rule, works from Gaming Mode;
- the SDDM selector elsewhere.

A start that fails before openSU draws its first frame selects the previous session again before
exiting, so `Relogin=true` can't loop.

## How to check

On the ROG Ally (`ssh ally`), the user installs session mode once from Desktop Mode, because the
polkit step needs their password. Then:

- switch in and out from both modes;
- kill openSU's Gamescope by PID inside the session and confirm the next login is the previous
  session.
