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
- Working notes with `file:line` citations: `scratch/iisu-re/notes/`.

## Chain

| Step | State | Evidence |
| --- | --- | --- |
| UI framework: Compose, rooted in `launcher/MainActivity` (extends `hw0`, a `ComponentActivity`) | partial | Compose classes referenced from 151 obfuscated files; the root composable is not yet located |
| Screen inventory and navigation graph | missing | — |
| Home grid geometry: rows, columns, feature tiles, pages, spacing | missing | — |
| Top bar and corner hints | missing | — |
| Tile composition: art fit, nine-patch frame, tint, logo, badges | missing | field names only, from R8 `toString()` (`b34`, `wx7`, `zm2`, `l34`, `f24`) |
| Focus treatment on a tile | missing | — |
| Input mapping and focus movement rules | missing | — |
| Sound effect and music mapping | missing | asset names only |
| Motion: focus, page, screen transitions, startup | missing | — |
| Palette | missing | — |

## Corrections to earlier notes

- iiSU is a Compose app. The earlier claim that it is not rested on one import
  count in un-obfuscated files.
- `assets/shaders/*.glsl` are AndroidX Media3's stock video-effect shaders, not
  iiSU colour grading, and say nothing about its look.
