# iideck

A gamepad-first shell for a game library. It shows Steam, Epic, GOG and
emulator ROMs in one grid and launches each title into the runtime that already
owns it.

It is not an emulator and ships no games. Steam, Legendary and Heroic keep doing
their own authentication, downloading and cloud sync; iideck reads what they have
already installed and hands launches back to them.

Built with C++20 and raylib. The shell is drawn from geometry rather than
composed from images, so the layout is resolution independent and the corner
radius is a fraction of tile size.

## Building

raylib is not a distro package here. `RAYLIB_ROOT` points at a prefix containing
`include/raylib.h` and `lib/libraylib.a`:

```sh
cmake -S . -B build/cmake -G Ninja -DCMAKE_CXX_COMPILER=clang++ \
      -DRAYLIB_ROOT=$HOME/dev/raylib-built
cmake --build build/cmake
```

The published `raylib-6.0_linux_amd64` binary cannot be used: it was built with
`STBI_REQUIRED` undefined and `SUPPORT_FILEFORMAT_JPG=0`, so `LoadImage` reports
"Data format not supported" for every file. Steam's artwork is JPEG, so raylib
has to be built from source with image decoding enabled. See
`$HOME/dev/raylib-built/BUILD-NOTES.md` for the exact flags.

Lucent is the project logger and is consumed from `LUCENT_ROOT`, which defaults
to `$HOME/repo/lucent`.

## Running

```sh
./build/cmake/src/iideck                  # the shell
./build/cmake/src/iideck --render out.png # one frame to a file, no window shown
```

Overrides, all optional:

| Variable | Meaning |
| --- | --- |
| `IIDECK_STEAM_ROOTS` | Colon-separated Steam install roots. Discovered when unset. |
| `IIDECK_ROM_ROOTS` | Colon-separated directories of emulator ROMs. |
| `IIDECK_EMULATORS` | `SYSTEM=program arg;SYSTEM2=program` |
| `IIDECK_WIDTH`, `IIDECK_HEIGHT` | Window size, default 1280x800. |
| `IIDECK_ASSETS` | Directory holding the typeface. Defaults to `assets`. |
| `IIDECK_GAMEPAD` | Only accept controllers whose name contains this. |

Run from the repository root, or set `IIDECK_ASSETS` if you start it elsewhere.

`IIDECK_GAMEPAD` exists because SDL reports any device with buttons as a
gamepad, so on a desktop with a multimedia keyboard the keyboard and its media
receiver show up as controllers. raylib offers no way to tell them apart, so
name yours.

A ROM whose system has no configured emulator still appears in the grid, and
pressing play reports which emulator is missing rather than doing nothing.

## Controls

| Input | Action |
| --- | --- |
| D-pad or left stick | Move focus |
| A | Play |
| Y / Select | Details |
| X | Refresh the library |
| LB / RB | Previous / next page |
| Start | Back to the first tile |

## Documentation

- `docs/project-goals.md` — what this is for, and what it deliberately is not
- `docs/project-state.md` — what works, what does not, and the current focus
- `docs/reference/design-reference.md` — the iiSU findings this design comes from

## Typeface

`assets/Nunito-Bold.ttf` is Nunito, the typeface the reference design uses,
under the SIL Open Font License; the licence is beside it in
`assets/Nunito-OFL.txt`. It ships as a static instance pinned to weight 700,
because Google Fonts publishes only the variable font and raylib reads glyphs
through stb_truetype, which ignores variation tables and would render the
light default master instead.