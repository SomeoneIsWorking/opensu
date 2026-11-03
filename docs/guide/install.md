# Installing openSU

Every [release](https://github.com/SomeoneIsWorking/opensu/releases) carries one package per
platform whose build passed, each with a `.sha256`; the release notes name any platform missing
from it. Check a download with `sha256sum -c <file>.sha256`. v0.3.0, the latest release, carries
the Linux AppImage, the Windows installer and the Android APK. The macOS app is built by CI but has
not shipped in a release yet.

## What each platform gets

| | Linux | Windows | macOS | Android |
| --- | --- | --- | --- | --- |
| Home, Library, search, details, Settings, sounds | yes | yes | yes | yes, plus touch |
| ROMs on the built-in libretro runner, shaders, setting layers, save states | yes | yes | yes | yes, the runner in its own Activity with touch controls |
| Standalone emulators | PATH, AppImages (downloadable), Flatpaks | found and launched | found in `/Applications` and launched | launched by intent |
| Steam | Normal, Managed, Isolated | Normal | Normal | no |
| Epic (Legendary), GOG (gogdl) | yes, Windows builds under Wine | yes, native builds | yes, Mac builds | no |
| Lua plugins | yes | yes | yes | yes |
| Button mappings for other programs' pads (virtual pads) | yes | no | no | no |
| Gamescope: nested and login session, FSR, frame generation, Alt+F4 | yes | no | no | no |
| Power list, Bluetooth and brightness tabs | yes | no | no | no |

Elsewhere than Linux openSU acts as a launcher: it lowers its window when a game starts and raises
it for the Guide menu or when the game ends.

## Linux: the AppImage

`openSU-<version>-x86_64.AppImage`, with its `.sha256`, a `.zsync` for AppImageUpdate and the
sign-in extension `opensu-signin.xpi`:

```sh
sha256sum -c openSU-<version>-x86_64.AppImage.sha256
chmod +x openSU-<version>-x86_64.AppImage
./openSU-<version>-x86_64.AppImage
```

The AppImage holds openSU, its assets, the sign-in extension (`usr/share/opensu/opensu-signin.xpi`)
and the pinned Gamescope fork. It is signed with the key in
[`packaging/appimage/signing-key.asc`](../../packaging/appimage/signing-key.asc) (fingerprint
`0604 E249 E180 039C 3BA2 FEF3 419F 535F 9867 0291`);
`./openSU-<version>-x86_64.AppImage --appimage-signature` prints the signature. Session mode
(Power › Install session mode) needs an openSU installed from source: the AppImage's mount point
changes on every run, so a login session entry cannot name it.

Requirements, which a desktop install of a current distribution already has:

- x86_64 Linux with glibc 2.39 or newer
- OpenGL and Vulkan drivers for the GPU (Mesa or the vendor's), and Xwayland
- FUSE 3 (`fusermount3`); libfuse2 is not needed, the runtime is the static type 2 one
- libcurl, libcrypto (OpenSSL 3), the PipeWire, ALSA and PulseAudio client libraries and a systemd
  user session

On Fedora: `sudo dnf install fuse3 libcurl openssl-libs pipewire-libs alsa-lib pulseaudio-libs
mesa-vulkan-drivers xorg-x11-server-Xwayland`. On Ubuntu: `sudo apt install fuse3 libcurl4t64
libssl3t64 libpipewire-0.3-0t64 libasound2t64 libpulse0 mesa-vulkan-drivers xwayland`.

Tested on Fedora 44 and Ubuntu 24.04 (clean containers with only the runtime libraries above: the
AppImage mounts, every bundled library resolves, the Gamescope binary loads and openSU renders a
frame; a Gamescope session itself is not run there). Other distributions with glibc 2.39 or newer
should work but are untested. On Fedora the dynamic loader prints "no version information
available" for libcurl on every start, since Fedora's libcurl has no symbol versions; it is
harmless.

`uv run --frozen python -m tools.package appimage all` builds it in a podman container (Ubuntu
24.04, the glibc floor) and runs the same checks; the AppImage lands in `build/appimage/out/`.

To build and install from source instead, see [building.md](building.md).

## Windows

`openSU-<version>-x86_64-setup.exe`: a per-user installer (no administrator rights) for Windows 10
or newer on x64, with a Start-menu entry. It is not code-signed, so SmartScreen asks once before it
runs.

## macOS (not released yet)

`openSU-<version>-macos-arm64.zip`: `openSU.app` for Apple silicon, macOS 14 or newer. It is signed
ad hoc, not notarised, so the first start is right-click › Open. MoltenVK and librashader are inside
the app. Wine is not offered on macOS; a store's Mac builds are installed and run.

## Android

`openSU-<version>-android.apk` for arm64-v8a and x86_64, Android 9 or newer, signed with the
certificate whose SHA-256 is
`BDC838D582DDC695CDAB431D2053B636EB93A0C2E1C3F468DD002D1011E43238`. Install it with the system
installer (allow installs from your browser or file manager when asked). Add your ROM folders in
Settings › Library › Add a ROM folder, which opens Android's folder picker. Games run on the built-in
cores, downloaded from the buildbot's `android/latest/<abi>` folder; the runner draws touch
controls while no pad is connected, and Back or the pause button opens the Guide menu. Standalone
emulators that are installed are started by intent with the game. Steam, Epic, GOG and the sign-in
extension are not on Android.

## Where openSU keeps your files

| | Settings and data | Cache |
| --- | --- | --- |
| Linux | `$XDG_CONFIG_HOME/opensu`, `$XDG_DATA_HOME/opensu` (`~/.config/opensu`, `~/.local/share/opensu`) | `$XDG_CACHE_HOME/opensu` (`~/.cache/opensu`) |
| Windows | `%APPDATA%\openSU` | `%LOCALAPPDATA%\openSU\cache` |
| macOS | `~/Library/Application Support/openSU` | `~/Library/Caches/openSU` |
| Android | the app's own files folder | the app's cache folder |

Nothing is kept inside the AppImage or the app.
