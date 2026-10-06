# RE frontier: iiSU

What of iiSU's UI and UX has been recovered from its code, and what is still
guessed. G002 builds only on rows marked grounded.

## Source

- `iiSU-Alpha-7.4.apk`, release `0.0.7.4` of `iisu-network/iiSU`, 128,322,093
  bytes, sha256 `0e9008b2b66c48f98edb7cfa42b3dbde2185ea439179ac673ed2e360769de6e8`.
  Fetch with `gh release download 0.0.7.4 -R iisu-network/iiSU -p iiSU-Alpha-7.4.apk`.
- Decompiled with jadx 1.5 into `scratch/iisu-re/jadx/` (gitignored; nothing
  from the APK enters git). App code is 56 classes under `com/iisulauncher`;
  the UI is Jetpack Compose, R8-obfuscated into `defpackage/`.
- Evidence with `file:line` citations: `reference/iisu/`; summary in `reference/design-reference.md`.

## Chain

| Step | State | Evidence |
| --- | --- | --- |
| UI framework: Compose root, custom Canvas tile renderer ("Browser2") | grounded | `reference/iisu/screens.md` §1 |
| Screen inventory, dialog router, settings pages, onboarding steps | grounded | `reference/iisu/screens.md` §2–5; ROM section and detail screen did not decompile |
| Home grid geometry: rows, columns, gap, fill order, paging, page pill | grounded | `reference/iisu/home-grid.md` §1 |
| Top bar and corner hints | partial | parts and some sizes; base sizing in undecompiled methods (`home-grid.md` §2) |
| Tile composition: chrome, frame nine-patch, art fit, tint, logo, focus ring | grounded | `reference/iisu/home-grid.md` §3; logo visibility condition inverted by jadx |
| "Jump back in!" and "Been a while" tiles: size and game choice | missing | code did not decompile |
| Palette | grounded | `reference/iisu/home-grid.md` §4 |
| Input routing, section cycling, grid focus rules, repeat, triggers | grounded | `reference/iisu/input-sound.md` §1; crossing to the next page not decoded |
| Per-screen key maps for Compose screens | partial | `reference/iisu/input-sound.md` §6, not re-read against source |
| Haptics | grounded | `reference/iisu/input-sound.md` §2 |
| Sound effects and domino cue | grounded | `reference/iisu/input-sound.md` §3; whether grid moves play `Navigation.wav` is unconfirmed |
| Music presets, loop windows, fades | grounded | `reference/iisu/input-sound.md` §4 |
| Motion: focus, domino entrance, pulse, dialogs, startup | grounded | `reference/iisu/motion.md` §1–4 |
| Motion: title pill text, clock, idle pause | missing | `reference/iisu/motion.md` §6 |

Spot-checked against the decompile: setContent call, dialog state names, 3×4
default grid, gap defaults, page-dot colour, focus scale timings, trigger
thresholds, music loop window, D-pad repeat throttle.

## Corrections to earlier notes

- iiSU is a Compose app. The earlier claim that it is not rested on one import
  count in un-obfuscated files.
- `assets/shaders/*.glsl` are AndroidX Media3's stock video-effect shaders, not
  iiSU colour grading, and say nothing about its look.
- `domino_icons_*` are not friend-count notification tiers; they are the section
  change cue, chosen by how many tiles the destination shows.
- iiSU does not promote titles to feature tiles. Wide and tall tiles come from
  widgets and user resizing.
