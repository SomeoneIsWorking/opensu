# iideck

A gamepad-first shell for a game library. It shows Steam, Epic, GOG and emulator
ROMs in one grid and launches each title into the runtime that already owns it.

It is not an emulator and it ships no games. Steam, Legendary and Heroic keep
doing their own authentication, downloading and cloud sync; iideck reads what
they have already installed and hands launches back to them.

## Building

```sh
./build.sh
```

The build needs `webkit2_41` because Fedora 44 ships webkit2gtk-4.1 and no
longer provides the 4.0 pkg-config file Wails defaults to. `build.sh` passes the
tag.

## Running

The window is fullscreen and frameless by default. Overrides, all optional:

| Variable | Meaning |
| --- | --- |
| `IIDECK_STEAM_ROOTS` | Colon-separated Steam install roots. Discovered when unset. |
| `IIDECK_ROM_ROOTS` | Colon-separated directories of emulator ROMs. |
| `IIDECK_EMULATORS` | `SYSTEM=program|arg|arg;SYSTEM2=program`, e.g. `SNES=snes9x-gtk\|-fullscreen` |
| `IIDECK_WIDTH`, `IIDECK_HEIGHT` | Window size, default 1280x800. |
| `IIDECK_FULLSCREEN` | `false` to run windowed. |
| `IIDECK_CHROME` | `true` to keep the window decorated. |
| `IIDECK_DEBUG` | `true` for debug-level logging. |

A ROM whose system has no configured emulator still appears in the grid, and
pressing play reports which emulator is missing rather than doing nothing.

## Controls

| Input | Action |
| --- | --- |
| D-pad or left stick | Move focus |
| A | Play |
| Y | Details |
| X | Refresh the library |
| LB / RB | Previous / next page |
| Start | Back to the first tile |

Keyboard mirrors the pad: arrows, Enter or Z to play, Y for details, X to
refresh, Escape to bring the window back after a game exits.

## Layout

The grid follows the reference home screen: a rounded top bar with the focused
title in a centre pill, square tiles mixed with multi-column feature tiles, page
dots, and button hints in the corners. Corner rounding is a percentage of tile
size, so the shape holds at any panel resolution. See
`docs/reference/design-reference.md` for what that reference is and what was
recovered from it.

Titles with no artwork get a generated card coloured from the title, so the grid
never shows a hole.

## Documentation

- `docs/project-goals.md` — what this is for, and what it deliberately is not
- `docs/project-state.md` — what works, what does not, and the current focus
- `docs/reference/design-reference.md` — the iiSU findings this design comes from