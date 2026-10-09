# openSU

A gamepad-first shell for a game library. It shows Steam, Epic, GOG and
emulator ROMs in one grid and launches each title into the runtime that already
owns it.

It is not an emulator and ships no games. Steam and Legendary keep doing their own
authentication, downloading and cloud sync; openSU reads what they have installed and
hands launches back to them. GOG is signed in by openSU itself (see Signing in to GOG and
Epic), its owned games are listed, and `gogdl` downloads them (see below).

openSU runs Steam in the background with `-cef-enable-debugging`, which opens Steam's
DevTools on 127.0.0.1:8080; that is how it reads download progress and starts
installs through Steam's own installer.

Built with C++20 and raylib. The shell is drawn from geometry rather than
composed from images, so the layout is resolution independent and every size is
derived from the window, following iiSU's own layout rules.

## Building

openSU needs raylib 6.0 built from source, a checkout of
[Lucent](https://github.com/SomeoneIsWorking/lucent), its logger, and
[nanosvg](https://github.com/memononen/nanosvg) for its icons: the distribution's
package (`nanosvg-devel` on Fedora), else a checkout at `NANOSVG_ROOT`, by default
`$HOME/dev/nanosvg`. The launcher logos are from [Simple Icons](https://simpleicons.org)
(CC0). libcurl (`libcurl-devel` on Fedora) downloads missing artwork; zlib (`zlib-devel`) reads
iiSU's starter pack and libwebp (`libwebp-devel`) decodes its console cards.

The published `raylib-6.0_linux_amd64` binary cannot be used: it was built with
`STBI_REQUIRED` undefined and `SUPPORT_FILEFORMAT_JPG=0`, so `LoadImage` reports
"Data format not supported" for every file, and Steam's artwork is JPEG. Build it
with image decoding on and X11 only (openSU reads its X11 window to become
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

Then point openSU at both (`RAYLIB_ROOT` defaults to `$HOME/dev/raylib-built`,
`LUCENT_ROOT` to `$HOME/repo/lucent`):

```sh
cmake -S . -B build/cmake -G Ninja -DCMAKE_CXX_COMPILER=clang++ \
      -DRAYLIB_ROOT="$PWD/raylib-prefix" -DLUCENT_ROOT="$PWD/lucent"
cmake --build build/cmake
```

### Gamescope fork

The nested session (openSU started on a desktop) runs openSU's own pinned
[Gamescope fork](https://github.com/SomeoneIsWorking/gamescope) (branch `opensu`), built by
`cmake/Gamescope.cmake` from a fixed commit. It adds `--close-focused-window`, so Alt+F4 in KDE
closes the game instead of ending the session. The fork is built with meson and ninja inside a
rootless podman container, so the host needs no Gamescope build dependencies, only podman
(`sudo dnf install podman`) and the network once. The container image
(`packaging/gamescope-build/Containerfile`) is the host's own Fedora release plus
`dnf builddep gamescope`, clang, meson, ninja and glslang, tagged by the Containerfile's hash; the
first `cmake --build` pulls and builds it (a few GB of image, several minutes), clones the commit
and its submodules, and builds the fork. Afterwards nothing is rebuilt until the Containerfile
changes. Only Fedora hosts are supported; configure refuses elsewhere. The staged binary's
runtime libraries are checked on the host with `ldd`; `sudo dnf install gamescope` installs any
that are missing. To work on the rest of openSU without the fork, configure with
`-DOPENSU_BUILD_GAMESCOPE=OFF`; the
nested session then refuses to start and says so. `cmake --install` puts the binary at
`<prefix>/libexec/opensu/gamescope`, where an installed openSU looks; a build-tree openSU looks at
`<build>/libexec/opensu/gamescope`. There is no `PATH` lookup and no fallback to the distribution's
Gamescope.

## Running

```sh
./build/cmake/src/opensu                  # the shell
./build/cmake/src/opensu --render out.png # one frame to a file, no window shown
./build/cmake/src/opensu --hidden         # maintainer run: unmapped window, no pads, free control port
```

`--hidden` logs its control channel port; drive it with `POST /input` and read `GET /state`.

Overrides, all optional:

| Variable | Meaning |
| --- | --- |
| `OPENSU_STEAM_ROOTS` | Colon-separated Steam install roots. Discovered when unset. |
| `OPENSU_ROM_ROOTS` | Colon-separated ROM roots, each holding one folder per system. Discovered when unset. |
| `OPENSU_EMULATORS` | Per-system command overriding the one found: `ps2=pcsx2-qt -batch {rom};gc=dolphin-emu -b -e {rom}`. |
| `OPENSU_WIDTH`, `OPENSU_HEIGHT` | Window size, default 1280x800. |
| `OPENSU_ASSETS` | Directory holding the typeface. Defaults to `share/opensu` beside the executable's directory. |
| `OPENSU_HOME_MODE` | `standard` (scrolling grid, default) or `wiisu` (paged grid). |

The build stages the assets beside the binary, so it runs from any directory.

## Installing

Configure with the prefix to install to, then install. The binary, its assets and a
desktop entry (`share/applications/opensu.desktop`, which shows openSU in the
application menu) go under the prefix:

```sh
cmake -S . -B build/cmake -G Ninja -DCMAKE_CXX_COMPILER=clang++ \
      -DRAYLIB_ROOT="$PWD/raylib-prefix" -DLUCENT_ROOT="$PWD/lucent" \
      -DCMAKE_INSTALL_PREFIX="$HOME/.local"
cmake --build build/cmake
cmake --install build/cmake
```

For a desktop icon as well, copy the entry to the desktop and mark it executable:
`cp "$HOME/.local/share/applications/opensu.desktop" "$(xdg-user-dir DESKTOP)/" && chmod +x "$(xdg-user-dir DESKTOP)/opensu.desktop"`.

Controllers are read from evdev: any pad whose driver follows the kernel's gamepad
codes (xpad, xone, hid-playstation, hid-nintendo, ...) works, keyboards are never
taken for one, and a pad switched on while openSU runs is picked up.

ROMs live in a ROM root (a folder named `ROM`, `ROMs` or `roms` in the home folder, in
`~/Emulation`, or at the top of a mounted drive) with one folder per system (`PS2`, `GameCube`,
`Switch`, `PSX CHD`, ...). A system folder holds game files or game folders; in a folder, the
base game is started rather than its updates and DLC. Emulators are found on PATH, as AppImages
in `~/Applications`, `~/AppImages` or `~/.local/bin`, or as Flatpaks: Dolphin, Cemu, Eden,
Ryujinx, PCSX2, RPCS3, shadPS4, DuckStation, PPSSPP, melonDS, Azahar, mGBA, xemu and Xenia
Canary. A game whose system has no emulator still appears, and pressing play names the
emulator to install.

## Signing in to GOG and Epic

Sign-in happens in your own browser, on the store's own page. openSU opens the page with
`xdg-open`, and a small browser extension hands the code the page ends on to openSU's control
channel and closes the tab. The build packs it as `~/.local/share/opensu/opensu-signin.xpi`.

Zen does not require signed extensions, so it can be installed for good: close Zen, copy the
`.xpi` to `<profile>/extensions/opensu-signin@opensu.xpi` (the Flatpak's profiles are under
`~/.var/app/app.zen_browser.zen/.zen/`), and add to the profile's `user.js`:

```js
user_pref("xpinstall.signatures.required", false);
user_pref("extensions.autoDisableScopes", 14);  // enable add-ons placed in this profile
```

Firefox release builds enforce signing, so there it can only be loaded per session:
`about:debugging` → This Firefox → Load Temporary Add-on → the `.xpi` (Ctrl+L takes a typed
path). A Flatpak browser cannot load the bare `manifest.json`: its file picker hands over only the
chosen file, not the scripts beside it. The extension talks to port 7311, openSU's default `OPENSU_CONTROL_PORT`.

```sh
curl -X POST http://127.0.0.1:7311/signin/gog/start   # opens GOG's sign-in page
curl -X POST http://127.0.0.1:7311/signin/epic/start  # opens Epic's, through Legendary's login
```

`POST /signin/gog` and `POST /signin/epic` take the authorization code as the body; the
extension does that for you. Epic's code is given to `legendary auth --code`. GOG's token is
kept in `$XDG_DATA_HOME/opensu/gog-token.json` (default `~/.local/share/opensu`), readable by
you only.

## Installing GOG games

GOG downloads go through `gogdl`, Heroic's standalone GOG downloader (no Heroic launcher).
Install it once, without root, pinned to the revision openSU was written against:

```sh
uv tool install "git+https://github.com/Heroic-Games-Launcher/heroic-gogdl@ac1580aeb004dda75557c97be9cc3105d50eeecb"
```

It needs a C compiler, for its xdelta3 extension. A game with a Linux build is downloaded as
that build; otherwise the Windows build is downloaded and run with `wine`. Games land in
`$XDG_DATA_HOME/opensu/gog-games/<id>/`.

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
`docs/reference/`; they are not needed to build or run opensu.

## Typeface

`assets/CalSans-Regular.ttf` is Cal Sans, the typeface iiSU uses by default, under the
SIL Open Font License; the licence is beside it in `assets/CalSans-OFL.txt`.

## License

openSU is MIT-licensed (`LICENSE`). Cal Sans is under the SIL Open Font License
(`assets/CalSans-OFL.txt`). openSU is an independent project, not affiliated with iiSU,
and ships none of iiSU's code or assets.
