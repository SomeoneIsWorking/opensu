# VM harness

`tools/vm/` runs a Fedora 44 KDE guest headless under qemu/KVM (user-mode networking, no root, no
libvirt network) to test openSU's login-session features for real. Images and downloads live in
`/mnt/Boy/vm/opensu/` (`OPENSU_VM_DIR`); screenshots in `scratch/vm/shots/` (`OPENSU_VM_SHOTS`).

Run everything as `uv run --frozen python -m tools.vm <command>`. Tests: `uv run --frozen pytest`;
lint: `uv run --frozen ruff check tools`.

## Commands

| Command | Does |
| --- | --- |
| `provision` | Kickstart install from the official mirror (kernel and initrd checked against the tree's `.treeinfo` sha256), SDDM and Plasma, user `tester` (wheel, password in `config.py`, sudo asks for it), then copies the host's `steamos-session-picker`, `steamos-session-select`, `steam-picker.desktop` and `kde_settings.conf` (autologin user retargeted), stubs `steam`, runs `dnf upgrade` and saves `base.qcow2` |
| `up [--gpu virtio\|venus] [--autologin off\|SESSION] [--snapshot NAME]` | Boots a fresh qcow2 overlay of the base (or a snapshot); `--autologin` writes an SDDM drop-in and reboots |
| `deploy [--headless-gamescope]` | Builds the current tree in `build/vm` (ccache temp under the VM dir, never `/run/user`), `DESTDIR`-installs with prefix `/home/tester/.local`, adds the pinned Gamescope fork from `build/clang`, installs the runtime packages the host `ldd` names, unpacks into the guest |
| `session-entry` | The README's `sudo install` line, run as the test user |
| `autologin off\|default\|SESSION` | Rewrites the autologin drop-in and reboots |
| `shot NAME`, `key CHORD...`, `type TEXT`, `click X Y` | QMP screendump (VNC frame when the GPU is GL) and keyboard and tablet input |
| `ui state\|press BUTTON...\|key CHORD\|text TEXT\|frame NAME` | openSU's control channel (port 7311, also served under `--session`) from inside the guest; `text` types into a focused field as a keyboard would (the install dialog's password); `frame` is the drawn shell, whatever the display shows |
| `ssh [--sudo] CMD...` | A command in the guest |
| `snapshot NAME` | Flattens the stopped VM's overlay into `snap-NAME.qcow2` |
| `down`, `status` | ACPI power-off then quit then kill of the recorded PID |

Every `up` starts from a clean overlay, so a test never inherits the last one's sessions. Restarting
SDDM instead of rebooting leaves the old user manager and compositor behind and corrupts a run.

## Failure runs

The real DRM Gamescope fails in the guest, which tests an abnormal login-session end: `up --gpu
virtio`, `deploy` (no shim), `ssh --sudo sh ~/.local/libexec/opensu/install-session.sh`, `ssh sudo
-n /usr/local/libexec/opensu-session-select opensu`, reboot. Expect one openSU login, then
`steamos-session-picker`, and `/etc/sddm.conf.d/zz-opensu-session.conf` gone.

Nested Gamescope does not run on the guest's KWin, so for the in-Plasma dialogs run the shell
itself from `ssh`: `OPENSU_SESSION=x WAYLAND_DISPLAY=wayland-0 DISPLAY=:0 XAUTHORITY=/run/user/1000/xauth_*
XDG_RUNTIME_DIR=/run/user/1000 ~/.local/bin/opensu`, then `ui press guide down down down down down a`
opens the power list.

## GPU

- `virtio` (`virtio-gpu-pci`, `-display none`): KMS and QMP screendump work, Plasma and the SDDM
  greeter run on llvmpipe. Gamescope refuses lavapipe on DRM ("not a valid physical device").
- `venus` (`virtio-gpu-gl-pci,blob=true,venus=true`, `-display egl-headless`): the guest gets the
  host's RADV through Venus, but QMP screendump has no surface; `shot` reads a VNC unix socket
  instead. Gamescope on DRM selects the Venus device, then aborts in `vn_ring_submit_locked` creating
  its output image: virglrenderer logs `failed to import resource: invalid res_id` for the dma-buf.
- So `--backend drm` cannot run in this guest. `deploy --headless-gamescope` installs a shim that
  rewrites `drm` to `headless` for the fork (everything else of the session is the shipped path);
  the display shows nothing of openSU, so use `ui frame`.

## Verifying a login-session change

1. `up --gpu virtio`, `deploy --headless-gamescope`, `session-entry`.
2. `autologin off` for a greeter run (`click 110 783` opens the session list), or
   `autologin opensu` for an autologin run.
3. Drive openSU with `ui press ...`; check the session with `ssh 'loginctl list-sessions'` and
   `~/.cache/wayland-errors` (the session's stdout).
