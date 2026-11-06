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
Add a game, libX11 and libXi (`libX11-devel libXi-devel`) read the in-game shortcut keys, and on
Linux sd-bus from libsystemd (`systemd-devel`) serves openSU's own polkit agent.

Remote play on Linux captures Gamescope and the game's sound through PipeWire (`pipewire-devel`),
codes the sound with Opus (`opus-devel`), and plays the guest's keys and pointer through XTest
(`libXtst-devel`); Debian and Ubuntu name them `libpipewire-0.3-dev libopus-dev libxtst-dev`. Its
video codec is PyroWave, from the pinned [fork](https://github.com/SomeoneIsWorking/pyrowave) and
pinned Granite (`-DOPENSU_BUILD_PYROWAVE=OFF` builds without it). For links too slow for PyroWave
it codes H.264, HEVC or AV1 through libavcodec on VA-API (`libavcodec-free-devel
libavutil-free-devel libva-devel` on Fedora, `libavcodec-dev libavutil-dev libva-dev` on Debian and
Ubuntu; `-DOPENSU_BUILD_VAAPI=OFF` builds without them). Which hardware codecs work is the VA-API
driver's: Fedora's own Mesa encodes none, RPM Fusion's `mesa-va-drivers-freeworld` encodes H.264
and HEVC. Built with neither, remote play has no codec to stream with.

`opensu-retro`, the built-in libretro runner, also needs the Vulkan loader and glslc
(`vulkan-loader-devel glslc` on Fedora), and Rust (`cargo`) for librashader, the shader preset
library, from the pinned [fork](https://github.com/SomeoneIsWorking/librashader). Its tests and the shell's draw on any Vulkan driver, lavapipe
(`mesa-vulkan-drivers`) included, and use the validation layers (`vulkan-validation-layers`) when
installed.

## The checked build

`uv run --frozen python -m tools.check all` checks out Lucent and gpu2d at the commits
`tools/check/pins.py` pins into `build/check/deps`, then configures, builds, lints (clang-format,
clang-tidy, ruff) and tests openSU with Clang in `build/check`, as CI does (`--gamescope off` leaves
the fork out of the tree). Each step also runs on its own (`deps`, `configure`, `build`, `lint`, `test`), and
`--build-dir` names another tree (`build/agent`, `build/player`). `--platform windows` builds on a
Windows host with clang-cl and vcpkg, `--platform macos` on an Apple silicon Mac with Homebrew's
LLVM and vcpkg. `--jobs N` caps the compilers and clang-tidy processes that run at once, the fork's
ninja included (one per core by default).

One build, lint, test or package run goes at a time on a machine, across every agent, always at full
width: a run takes 4 to 5 GB, and several at once run the machine out of memory. Work in parallel is
edits only, in source-only worktrees with no build tree of their own. To check a commit, put it in
the one shared checkout and tree when no other run is going:

```
git -C build/agent-src switch --detach <commit>
uv run --directory build/agent-src --frozen python -m tools.check build --build-dir ../agent
```

Hidden proof runs use the same tree's binaries. Whoever coordinates the agents gives out the turns;
no `--jobs` cap, memory guard or queue stands in for that.

On Linux the host needs podman and none of the build tools or dev packages: `deps`, `configure`,
`build` and `lint` run in the build image, `packaging/ci/Containerfile` on the host's own Fedora
release (from `/etc/os-release`), so the binaries link against runtime libraries the host already
has. The image has Clang, CMake, Ninja, Rust and every package openSU and the fork build against;
it is tagged by the Containerfile's hash and the release, built on the first run (a few GB, several
minutes) and reused until either changes. The steps run as the calling user with the checkout, its
git common dir and the shared third-party prefix mounted at their own paths, so the tree configured
inside is the same tree outside, and everything they write stays in the build directory (Cargo's
and uv's caches too). The fork builds natively inside it. `test` runs on the host, where openSU
runs. The container is always used rather than only when the host lacks a package: one environment,
the one CI uses, builds every tree, so a tree never changes compiler or headers because the host's
packages did. A host without a working rootless podman is refused with the install command per
distribution; a host other than Fedora is refused, since the image's binaries need Fedora's
libraries, and builds by hand instead. CI's `tools.check ci` runs `all` in the same image on the
pinned Fedora 44, with systemd as PID 1 for the tests.

PyroWave, librashader and the Gamescope and SDL forks are not built in a build tree. `deps` builds
`cmake/thirdparty` once per pinned revision, toolchain and build image into a prefix of their own,
`~/.local/share/opensu-thirdparty/<platform>-<digest>` (the digest covers `cmake/thirdparty`, the
pins in `cmake/ThirdParty.cmake`, `cmake/toolchains`, the compiler arguments and versions, the
build image's tag, which each image's run names in `OPENSU_BUILD_IMAGE`, and `rustc -vV`), and
`configure` hands it to CMake as `CMAKE_PREFIX_PATH`, where `find_package(OpensuPyroWave)`,
`find_package(OpensuLibrashader)`, `find_package(OpensuGamescope)` and `find_package(OpensuSDL)`
find them; every build tree
and checkout with the same pins, compilers and image shares it. An Android build does the same per
ABI with the NDK's toolchain.

The tool checks the checkout its own code is in, which `python -m` takes from the current
directory, so another checkout is named with `uv run --frozen --directory <checkout> python -m
tools.check ...`. It refuses a run in another checkout's environment (`uv run --project`), a build
tree configured from another checkout, and a lint whose file lists come out empty.

`tools.check lint --changed [--base origin/main]` is the quick lint while working; plain `lint` stays
the full check, which CI runs. It takes the files that differ from the merge base with `--base`,
uncommitted and untracked ones included. clang-format and ruff get those files, clang-tidy the
changed translation units and every one that includes a changed header, from `ninja -t deps`, so
build first: a tree whose objects have no current record falls back to the full list and says so. A
changed `.clang-tidy`, `.clang-format` or CMake file means the full list too. Nothing changed is a
clean pass.

## By hand

Building outside `tools.check` needs every package listed under Dependencies installed; a missing
one stops configure with the distribution's package names.

```sh
git clone https://github.com/SomeoneIsWorking/lucent.git
git clone https://github.com/SomeoneIsWorking/gpu2d.git
```

Then point openSU at both (`LUCENT_ROOT` defaults to `$HOME/repo/lucent`, `GPU2D_ROOT` to
`$HOME/repo/gpu2d`)

and at a prefix holding PyroWave, librashader and the Gamescope and SDL forks, which
`cmake/thirdparty` builds (the forks need Gamescope's, SDL3's, SDL3_ttf's and SDL3_image's build dependencies,
`sudo dnf builddep gamescope SDL3 SDL3_ttf SDL3_image` on Fedora):

```sh
cmake -S cmake/thirdparty -B build/thirdparty -G Ninja -DCMAKE_C_COMPILER=clang \
      -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_INSTALL_PREFIX="$PWD/thirdparty"
cmake --build build/thirdparty && cmake --install build/thirdparty
cmake -S . -B build/cmake -G Ninja -DCMAKE_CXX_COMPILER=clang++ \
      -DLUCENT_ROOT="$PWD/lucent" -DGPU2D_ROOT="$PWD/gpu2d" -DCMAKE_PREFIX_PATH="$PWD/thirdparty"
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
[Gamescope fork](https://github.com/SomeoneIsWorking/gamescope) (branch `opensu`), pinned in
`cmake/ThirdParty.cmake`. It adds `--close-focused-window`, so Alt+F4 in KDE closes the game
instead of ending the session, and openSU's frame generation. The shared third-party prefix builds
it (`cmake/thirdparty/Gamescope.cmake`: meson, release, Clang, only `src/gamescope`) in the build
image, which has its build dependencies, with `--jobs` capping the fork's ninja; a build tree only
copies the binary (`cmake/Gamescope.cmake`), so a second tree builds none of it and a pin or image
change builds it once more in a new prefix. The staged binary's runtime libraries are checked with
`ldd`; `sudo dnf install gamescope` installs any that are missing.

To work on the rest of openSU without the fork, configure with `-DOPENSU_BUILD_GAMESCOPE=OFF`; the
nested session then refuses to start and says so. `cmake --install` puts the binary at
`<prefix>/libexec/opensu/gamescope`, where an installed openSU looks; a build-tree openSU looks at
`<build>/libexec/opensu/gamescope`. There is no `PATH` lookup and no fallback to the distribution's
Gamescope.

## The SDL fork

On Linux the shell and the runner use openSU's pinned
[SDL fork](https://github.com/SomeoneIsWorking/SDL) (branch `opensu/release-3.4.18`): SDL 3.4.18
whose GPU API also wraps a DMA-BUF plane as a Vulkan texture, so remote play's guest draws the
hardware decoder's picture without copying it through the CPU. The shared third-party prefix builds
it (`cmake/thirdparty/SDL.cmake`) under `opensu-sdl/`, with the pinned SDL_ttf and SDL_image
releases built against it, so no library openSU links brings the system's SDL3 with it;
`cmake/SDL.cmake` puts their pkg-config directory first and installs `libSDL3.so.0`,
`libSDL3_ttf.so.0` and `libSDL3_image.so.0` into `<prefix>/lib64/opensu`, which the executables'
RPATH names. A build-tree
executable loads it from the prefix. `-DOPENSU_BUILD_SDL_FORK=OFF` builds against the system's
SDL3 (the AppImage does, having no hardware decoder); the guest then reads each picture back.

## Installing from source

`uv run --frozen python -m tools.check install --build-dir build/player` installs a built tree
into `~/.local` (`--prefix` for another) from the build image: the binaries, the fork, the assets,
the icon and the application menu entry (`share/applications/opensu.desktop`), then copies that
entry to the desktop (`xdg-user-dir DESKTOP`) as the desktop shortcut, so both run the installed
`<prefix>/bin/opensu`. It writes the shortcut again on every install; `--destdir` stages an install
under another root and writes none.

By hand, install the tree with `cmake --install build/cmake --prefix "$HOME/.local"`. The menu
entry is written at install time, so its `Exec` names the prefix the install went to, whatever the
tree was configured with (`-DOPENSU_DESKTOP_EXEC` replaces it, as the AppImage's `opensu` does).
For a desktop icon as well, copy the entry to the desktop and mark it executable:
`cp "$HOME/.local/share/applications/opensu.desktop" "$(xdg-user-dir DESKTOP)/" && chmod +x
"$(xdg-user-dir DESKTOP)/opensu.desktop"`.

Session mode needs nothing from the install: openSU stages its root step itself, wherever it
runs from; see [session.md](session.md).

## Packages

`tools.package` builds each release package: `appimage` (in an Ubuntu 24.04 podman container),
`windows` (the Inno Setup installer), `macos` (the zipped app) and `android` (the signed APK through
the pinned android-port project), each into an `out/` folder of its build tree.
