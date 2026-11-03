# Gamescope: the nested session and the login session

On Linux openSU runs games inside its own pinned
[Gamescope fork](https://github.com/SomeoneIsWorking/gamescope) ([building.md](building.md#the-gamescope-fork)).
Started from a desktop, openSU starts that Gamescope nested in the desktop, and Steam and games run
inside it; Alt+F4 in KDE closes the game in front instead of ending the session. With `--session`
openSU is the login session itself. Windows, macOS and Android have no Gamescope; there openSU is a
launcher ([install.md](install.md#what-each-platform-gets)).

## Resolution, upscaling and frame generation

Inside openSU's Gamescope every game has, in its setting layers ([libretro.md](libretro.md#setting-layers)):

- **Resolution and upscaling**: Resolution (Full, or 5/6, 3/4, 2/3, 1/2 of the screen, keeping its
  shape), Upscaling (Linear, AMD FSR, NVIDIA Image Scaling, Nearest, Integer) and Sharpness for FSR
  and NIS. The game draws at that size and Gamescope upscales it to the screen.
- **Frame generation**, off by default: a preset (Off, Smooth 60, Match display) or Custom, fixed
  ratios or adaptive to a target frame rate. It runs the frame-generation shaders of your own
  Lossless Scaling from Steam (its `Lossless.dll`, read in place), on the game in front.

Outside openSU's Gamescope the rows still show, marked "applies in openSU's Gamescope session".

## The login session

`--session` makes openSU the login session: from a display manager it starts the pinned Gamescope
top level on the seat (`--backend drm`, the monitor's own mode) with openSU in it, and there is no
desktop underneath. Closing openSU ends the session.

From the desktop, open the Guide menu, then Power. **Install session mode** explains what it is and
what it installs, and on Accept asks for your administrator password inside openSU (a masked field,
the on-screen keyboard or your keyboard; it goes to `sudo` on stdin and is never stored or logged).
It runs one `sudo` step that puts three things in place:

| File | Why |
| --- | --- |
| `/usr/local/share/wayland-sessions/opensu.desktop` | the session entry SDDM lists |
| `/usr/local/libexec/opensu-session-select` | a small root-owned script that sets which session the next login uses (`opensu` or `restore`) |
| `/etc/sudoers.d/zz-opensu-session-select` | lets your user run exactly that script with exactly those two arguments, without a password |

Afterwards Power shows **Switch to session mode** (press A twice): it selects openSU for the next
login and logs the Plasma session out, and SDDM logs in to openSU. In the session, Power then
**Switch to desktop** selects the previous session again and ends openSU. With SDDM autologin the
selector writes `/etc/sddm.conf.d/zz-opensu-session.conf` (`[Autologin] Session=`, which wins over
`kde_settings.conf`) and removes it on restore; without autologin it sets SDDM's last session in
`/var/lib/sddm/state.conf`. What was there before is kept in `/var/lib/opensu/previous-session`.
Switching to session mode ends the session with KDE's `org.kde.Shutdown.logout`, so it needs
Plasma; on another desktop the selection is undone and the refusal shown.

Session mode needs an openSU installed from source: the AppImage's mount point changes on every
run, so a login session entry cannot name it. `cmake --install` also puts `install-session.sh` and
`opensu-session-select` in `<prefix>/libexec/opensu/`, and prints the same step for a terminal:

```sh
sudo sh ~/.local/libexec/opensu/install-session.sh
```

(for the `~/.local` prefix). Without the selector, Switch to desktop in a session entered through an
entry installed by hand still works: openSU leaves a note, ends its Gamescope and starts
`startplasma-wayland` in the session's place.
