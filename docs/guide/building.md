# Building openSU from source

openSU is C++20 on SDL3 and [gpu2d](https://github.com/SomeoneIsWorking/gpu2d), a 2D canvas on SDL3
GPU (Vulkan on Linux). The shell is drawn from geometry rather than composed from images, so the
layout is resolution independent and every size is derived from the window, following iiSU's own
layout rules.

## Dependencies

openSU needs checkouts of [Lucent](https://github.com/SomeoneIsWorking/lucent), its logger, and
[gpu2d](https://github.com/SomeoneIsWorking/gpu2d), its renderer, image codecs and text, and
[nanosvg](https://github.com/memononen/nanosvg) for its icons: the distribution's package
(`nanosvg-devel` on Fedora), else a checkout at `NANOSVG_ROOT`, by default `$HOME/dev/nanosvg`.
The store logos are from [Simple Icons](https://simpleicons.org) (CC0), in `assets/icons/`.

The window, keyboard, mouse and sound are SDL3's; gpu2d draws with SDL3 GPU and reads faces and
images through HarfBuzz, SDL3_ttf and SDL3_image (`SDL3-devel SDL3_ttf-devel harfbuzz-devel
SDL3_image-devel libpng-devel` on Fedora), and the UI sounds are Ogg Vorbis (`libvorbis-devel`).
libcurl (`libcurl-devel`) downloads missing artwork; libcrypto (`openssl-devel`) decrypts a Switch
game's own title and icon with the player's keys; zlib (`zlib-devel`) inflates zips, nlohmann/json
(`json-devel`) reads and writes the settings, SQLite (`sqlite-devel`) reads Lutris's game list for
Add a game, and libX11 and libXi (`libX11-devel libXi-devel`) read the in-game shortcut keys.

`opensu-retro`, the built-in libretro runner, also needs the Vulkan loader and glslc
(`vulkan-loader-devel glslc` on Fedora), and Rust (`cargo`) for librashader, the shader preset
library, built from the pinned [fork](https://github.com/SomeoneIsWorking/librashader) by
`cmake/Librashader.cmake`. Its tests and the shell's draw on any Vulkan driver, lavapipe
(`mesa-vulkan-drivers`) included, and use the validation layers (`vulkan-validation-layers`) when
installed.

## The checked build

`uv run --frozen python -m tools.check all` checks out Lucent and gpu2d at the commits
`tools/check/pins.py` pins into `build/check/deps`, then configures, builds, lints (clang-format,
clang-tidy, ruff) and tests openSU with Clang in `build/check`, as CI does (`--gamescope native`
when Gamescope's build dependencies are installed, `off` to skip the fork). Each step also runs on
its own (`deps`, `configure`, `build`, `lint`, `test`). `--platform windows` builds on a Windows host
with clang-cl and vcpkg, `--platform macos` on an Apple silicon Mac with Homebrew's LLVM and vcpkg.

## By hand

```sh
git clone https://github.com/SomeoneIsWorking/lucent.git
git clone https://github.com/SomeoneIsWorking/gpu2d.git
```

Then point openSU at both (`LUCENT_ROOT` defaults to `$HOME/repo/lucent`, `GPU2D_ROOT` to
`$HOME/repo/gpu2d`):

```sh
cmake -S . -B build/cmake -G Ninja -DCMAKE_CXX_COMPILER=clang++ \
      -DLUCENT_ROOT="$PWD/lucent" -DGPU2D_ROOT="$PWD/gpu2d"
cmake --build build/cmake
```

The build stages the assets beside the binary, so it runs from any directory:

```sh
./build/cmake/src/opensu                  # the shell
./build/cmake/src/opensu --render out.png # one frame to a file, no window shown
./build/cmake/src/opensu --hidden         # maintainer run: unmapped window, no pads, free control port
./build/cmake/src/opensu --session        # openSU is the login session, not an app on a desktop
```

`opensu --help` lists every flag; [environment.md](environment.md) has the environment and the
control channel.

## The Gamescope fork

The nested session (openSU started on a desktop) runs openSU's own pinned
[Gamescope fork](https://github.com/SomeoneIsWorking/gamescope) (branch `opensu`), built by
`cmake/Gamescope.cmake` from a fixed commit. It adds `--close-focused-window`, so Alt+F4 in KDE
closes the game instead of ending the session, and openSU's frame generation. The fork is built
with meson and ninja inside a rootless podman container, so the host needs no Gamescope build
dependencies, only podman (`sudo dnf install podman`) and the network once. The container image
(`packaging/gamescope-build/Containerfile`) is the host's own Fedora release plus
`dnf builddep gamescope`, clang, meson, ninja and glslang, tagged by the Containerfile's hash; the
first `cmake --build` pulls and builds it (a few GB of image, several minutes), clones the commit
and its submodules, and builds the fork. Afterwards nothing is rebuilt until the Containerfile
changes. Only Fedora hosts are supported; configure refuses elsewhere. The staged binary's runtime
libraries are checked on the host with `ldd`; `sudo dnf install gamescope` installs any that are
missing.

To work on the rest of openSU without the fork, configure with `-DOPENSU_BUILD_GAMESCOPE=OFF`; the
nested session then refuses to start and says so. `cmake --install` puts the binary at
`<prefix>/libexec/opensu/gamescope`, where an installed openSU looks; a build-tree openSU looks at
`<build>/libexec/opensu/gamescope`. There is no `PATH` lookup and no fallback to the distribution's
Gamescope.

## Installing from source

Configure with the prefix to install to, then install. The binary, its assets, its icon and a
desktop entry (`share/applications/opensu.desktop`, which shows openSU in the application menu) go
under the prefix:

```sh
cmake -S . -B build/cmake -G Ninja -DCMAKE_CXX_COMPILER=clang++ \
      -DLUCENT_ROOT="$PWD/lucent" -DGPU2D_ROOT="$PWD/gpu2d" \
      -DCMAKE_INSTALL_PREFIX="$HOME/.local"
cmake --build build/cmake
cmake --install build/cmake
```

For a desktop icon as well, copy the entry to the desktop and mark it executable:
`cp "$HOME/.local/share/applications/opensu.desktop" "$(xdg-user-dir DESKTOP)/" && chmod +x
"$(xdg-user-dir DESKTOP)/opensu.desktop"`.

`cmake --install` also puts the session mode's installer in `<prefix>/libexec/opensu/`; see
[session.md](session.md).

## Packages

`tools.package` builds each release package: `appimage` (in an Ubuntu 24.04 podman container),
`windows` (the Inno Setup installer), `macos` (the zipped app) and `android` (the signed APK through
the pinned android-port project), each into an `out/` folder of its build tree.
