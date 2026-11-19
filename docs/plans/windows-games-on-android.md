# Windows games on Android: GOG, Epic and Steam through Wine and FEX

Asked 2026-11-19: GOG and Epic games on Android, and local Steam play there through fumarole, all
run by Wine with FEX emulating x86. Today Android has no Steam, Epic or GOG at all
(`docs/project-state.md`): `legendary` and `gogdl` are Python programs that do not run there
(`library::cliStoresRun`), and the Steam client does not exist for it.

## Parts, in the order they are built

1. **Steam games through fumarole on Linux.** openSU starts an owned Steam game's Windows build
   with `fumarole launch` (its own `steamclient.dll` under Wine, cloud sync before and after)
   instead of the isolated Steam client, for a game that needs no Steam Input or anti-cheat: the
   game is openSU's own child, with no kept virtual pads for a sandbox, the sound sink's
   environment reaching it and no client to hide. Windows builds only until fumarole has a native
   `steamclient.so`. It is where fumarole is verified today, and every later part reuses it.
2. **GOG and Epic downloads without Python.** A C++ downloader per store in openSU: GOG's
   content-system v2 manifests and Epic's chunked manifests, signed in with the tokens openSU
   already holds (`gog-token.json`, Legendary's). It replaces `gogdl` and `legendary` on every
   platform, so the desktop drops its Python dependency too and Android gets the same code.
3. **The Android runtime.** A Wine build for Android (bionic) running natively on ARM64, with
   FEX's ARM64EC and WoW64 DLLs (`libarm64ecfex.dll`, `libwow64fex.dll`) emulating the game's
   x86-64 and x86 code inside it; DXVK and VKD3D-Proton over the device's Vulkan driver (Turnip
   on Adreno); a surface Wine draws into inside openSU's activity; the touch layer and pads
   feeding Wine's input. Downloaded at first use like the emulators, never shipped in the APK
   (`docs/plans/embedded-runner.md` for how runners are fetched).
4. **fumarole on Android.** fumarole built for Android ARM64, its Windows `steamclient.dll`
   inside the Android Wine, and `fumarole install` downloading the Windows depots there: a Steam
   game then installs and plays on the phone with no Steam client.

Parts 1 and 2 need no Android work and land first; part 3 is the long one and gates 4.

## Prior art to check before part 3

Each needs its licence and what it ships recorded in `docs/prior-art.md` before anything is
taken:

- FEX-Emu: the ARM64EC/WoW64 backend for Wine; upstream builds the DLLs.
- Winlator and its Bionic fork (Box64 or FEXCore ARM64EC containers), GameNative (Steam on Android
  through Wine), GameHub: how they build Wine for bionic, their X server and their driver packaging.
- Turnip (Mesa) builds for Adreno 7xx; the test phone is a Snapdragon 7 Gen 4 (SM7750,
  Adreno), Android 17, 8 GB.

## Known risks

- Wine ARM64EC is young: FEX's issue tracker has old games that need NX missing failing under it.
- 8 GB of RAM limits what runs; a game's own footprint plus Wine and FEX.
- Mali and other non-Adreno GPUs have no Turnip; they use the vendor's Vulkan driver, which DXVK
  may not run on.
