# iideck

A gamepad-first shell for a game library. It shows Steam, Epic, GOG and
emulator ROMs in one grid and launches each title into the runtime that already
owns it.

It is not an emulator and ships no games. Steam, Legendary and Heroic keep doing
their own authentication, downloading and cloud sync; iideck reads what they have
already installed and hands launches back to them.

iideck runs Steam in the background with `-cef-enable-debugging`, which opens Steam's
DevTools on 127.0.0.1:8080; that is how it reads download progress and starts
installs through Steam's own installer.

Built with C++20 and raylib. The shell is drawn from geometry rather than
composed from images, so the layout is resolution independent and every size is
derived from the window, following iiSU's own layout rules.

## Building

iideck needs raylib 6.0 built from source, a checkout of
[Lucent](https://github.com/SomeoneIsWorking/lucent), its logger, and
[nanosvg](https://github.com/memononen/nanosvg) for its icons: the distribution's
package (`nanosvg-devel` on Fedora), else a checkout at `NANOSVG_ROOT`, by default
`$HOME/dev/nanosvg`. The launcher logos are from [Simple Icons](https://simpleicons.org)
(CC0). libcurl (`libcurl-devel` on Fedora) downloads missing artwork.

The published `raylib-6.0_linux_amd64` binary cannot be used: it was built with
`STBI_REQUIRED` undefined and `SUPPORT_FILEFORMAT_JPG=0`, so `LoadImage` reports
"Data format not supported" for every file, and Steam's artwork is JPEG. Build it
with image decoding on and X11 only (iideck reads its X11 window to become
Gamescope's overlay), then install it to a prefix of its own:

```sh
git clone --branch 6.0 https://github.com/raysan5/raylib.git
cmake -S raylib -B raylib/build -DCMAKE_BUILD_TYPE=Release -DBUILD_EXAMPLES=OFF \
      -DGLFW_BUILD_WAYLAND=OFF -DGLFW_BUILD_X11=ON -DCMAKE_INSTALL_LIBDIR=lib \
      -DCMAKE_C_FLAGS="-DSTBI_REQUIRED -DSUPPORT_FILEFORMAT_JPG=1 -DSUPPORT_FILEFORMAT_TGA=1 -DSUPPORT_FILEFORMAT_PSD=1"
cmake --build raylib/build
cmake --install raylib/build --prefix "$PWD/raylib-prefix"
git clone https://github.com/SomeoneIsWorking/lucent.git
```

Then point iideck at both (`RAYLIB_ROOT` defaults to `$HOME/dev/raylib-built`,
`LUCENT_ROOT` to `$HOME/repo/lucent`):

```sh
cmake -S . -B build/cmake -G Ninja -DCMAKE_CXX_COMPILER=clang++ \
      -DRAYLIB_ROOT="$PWD/raylib-prefix" -DLUCENT_ROOT="$PWD/lucent"
cmake --build build/cmake
```

## Running

```sh
./build/cmake/src/iideck                  # the shell
./build/cmake/src/iideck --render out.png # one frame to a file, no window shown
```

Overrides, all optional:

| Variable | Meaning |
| --- | --- |
| `IIDECK_STEAM_ROOTS` | Colon-separated Steam install roots. Discovered when unset. |
| `IIDECK_ROM_ROOTS` | Colon-separated ROM roots, each holding one folder per system. Discovered when unset. |
| `IIDECK_EMULATORS` | Per-system command overriding the one found: `ps2=pcsx2-qt -batch {rom};gc=dolphin-emu -b -e {rom}`. |
| `IIDECK_WIDTH`, `IIDECK_HEIGHT` | Window size, default 1280x800. |
| `IIDECK_ASSETS` | Directory holding the typeface. Defaults to `share/iideck` beside the executable's directory. |
| `IIDECK_HOME_MODE` | `standard` (scrolling grid, default) or `wiisu` (paged grid). |

The build stages the assets beside the binary, so it runs from any directory.

## Installing

Configure with the prefix to install to, then install. The binary, its assets and a
desktop entry (`share/applications/iideck.desktop`, which shows iideck in the
application menu) go under the prefix:

```sh
cmake -S . -B build/cmake -G Ninja -DCMAKE_CXX_COMPILER=clang++ \
      -DRAYLIB_ROOT="$PWD/raylib-prefix" -DLUCENT_ROOT="$PWD/lucent" \
      -DCMAKE_INSTALL_PREFIX="$HOME/.local"
cmake --build build/cmake
cmake --install build/cmake
```

For a desktop icon as well, copy the entry to the desktop and mark it executable:
`cp "$HOME/.local/share/applications/iideck.desktop" "$(xdg-user-dir DESKTOP)/" && chmod +x "$(xdg-user-dir DESKTOP)/iideck.desktop"`.

Controllers are read from evdev: any pad whose driver follows the kernel's gamepad
codes (xpad, xone, hid-playstation, hid-nintendo, ...) works, keyboards are never
taken for one, and a pad switched on while iideck runs is picked up.

ROMs live in a ROM root (a folder named `ROM`, `ROMs` or `roms` in the home folder, in
`~/Emulation`, or at the top of a mounted drive) with one folder per system (`PS2`, `GameCube`,
`Switch`, `PSX CHD`, ...). A system folder holds game files or game folders; in a folder, the
base game is started rather than its updates and DLC. Emulators are found on PATH, as AppImages
in `~/Applications`, `~/AppImages` or `~/.local/bin`, or as Flatpaks: Dolphin, Cemu, Eden,
Ryujinx, PCSX2, RPCS3, shadPS4, DuckStation, PPSSPP, melonDS, Azahar, mGBA, xemu and Xenia
Canary. A game whose system has no emulator still appears, and pressing play names the
emulator to install.

## Controls

| Input | Action |
| --- | --- |
| D-pad or left stick | Move focus |
| A | Play, or install a Steam game that is not installed |
| Y / Select | Details |
| X | Refresh the library |
| LB / RB | Previous / next page (`wiisu` mode only) |
| Start | Back to the first tile |
| Guide (in a game) | Menu over the game: Resume or Close game; B or Guide resumes. The game gets no controller input while it is open |

## Documentation

- `docs/project-goals.md` — what this is for, and what it deliberately is not
- `docs/project-state.md` — what works, what does not, and the current focus
- `docs/re-frontier.md` — what of iiSU's UI has been recovered, and what is still guessed

The detailed reverse-engineering notes that UI comments cite (`home-grid.md`,
`motion.md`, ...) are kept in a private repository, checked out at the gitignored
`docs/reference/`; they are not needed to build or run iideck.

## Typeface

`assets/CalSans-Regular.ttf` is Cal Sans, the typeface iiSU uses by default, under the
SIL Open Font License; the licence is beside it in `assets/CalSans-OFL.txt`.

## License

iideck is MIT-licensed (`LICENSE`). Cal Sans is under the SIL Open Font License
(`assets/CalSans-OFL.txt`). iideck is an independent project, not affiliated with iiSU,
and ships none of iiSU's code or assets.
