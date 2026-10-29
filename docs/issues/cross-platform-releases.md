# Windows, macOS and Android releases beside the Linux AppImage

Status: open. Goal G005. Step 1 landed (S042, "Step 1 as built"); step 2, the Windows release,
landed (S046, "Step 2 as built"); step 3 (macOS) is open; step 4 (Android) is in progress
("Step 4 as built").

Requested: "I want Windows/Linux/macOS/Android releases, ofc the isolation and gamescope features
only work in Linux, other platforms just act as a launcher".

Before step 1 openSU configured only on Linux: the top-level `find_package(X11 REQUIRED)`, the
Gamescope fork, `opensu_session` linking `X11::Xi`, POSIX/Linux headers in 15+ translation units,
and gpu2d's window including `X11/Xlib.h`. CI and `release.yml` build Linux only.

## 1. What is Linux-only, by owner

| Owner | Linux-only part | Elsewhere |
| --- | --- | --- |
| `src/launch/process_launcher_*` (was `instance.*`) | One `systemd-run --user --scope` per launch, `systemctl stop` of the scope, signal mask, exit status | Windows: `CreateProcessW` in a Job Object (`KILL_ON_JOB_CLOSE`), which is a real counterpart: the tree stays owned. macOS: `posix_spawn` with its own process group. Android: an `Intent` (no tree to own) |
| `src/launch/command.*`, `argv.hpp` | `fork`/`execv`, `poll` on pipes, `pthread` sigmask | Windows: `CreateProcessW` with pipes and a quoted command line. macOS: the POSIX path as is. Android: none (only the runner and intents) |
| `src/launch/process_tree.*` | `/proc` scans by command line | Windows: Toolhelp32; macOS: `proc_listpids`/`sysctl`; only needed where Steam and Wine are, so Linux-only by use |
| `src/launch/handoff.*`, `game_windows.hpp` | Window-shown check through Gamescope's root properties | `GameWindows` is already an interface: Windows `EnumWindows` + `GetWindowThreadProcessId`, macOS `CGWindowListCopyWindowInfo` (owner pid needs no permission), Android: our Activity paused |
| `src/session/*` (all), `cmake/Gamescope.cmake`, `packaging/gamescope-build/` | Nested and DRM Gamescope, overlay atoms, XInput2 game keys, focus, Steam window hiding, X11 monitor read, restart budget | None. Not built outside Linux |
| `src/host/session_mode.*`, `login_session_end.*`, `desktop_request.*`, `packaging/install-session.sh`, `opensu-session-select.sh`, `opensu-session.desktop.in` | Login session, SDDM, sudo | None |
| `src/host/power.*`, `bluetooth.*`, `backlight.*`, `runner.*`; `app/bluetooth_session.*`, `brightness_control.*` | logind over `busctl`, `bluetoothctl`, sysfs backlight | Off; the OS's own settings own them |
| `src/device/battery.*` | controller batteries from sysfs | The system battery is `SDL_GetPowerInfo` on every OS since step 1; controller batteries are a Linux backend, none elsewhere (`SDL_GetGamepadPowerInfo` through `SdlPads` is the step 2 candidate) |
| `src/audio/volume_backend.*` | `wpctl`/`pactl` | Windows `IAudioEndpointVolume`, macOS CoreAudio, Android `AudioManager`, or off in the first release of each |
| `src/gamepad/evdev_device.*`, `virtual_pad.*`, `evdev_pads.*` (was `pads.*`), `pad_translator.*` (evdev in) | evdev reads, `EVIOCGRAB`, uinput Xbox 360 pads, `/sys` | SDL3 gamepad events into the same `Event`/`Button`; no grab, no virtual pads. `chord_gate`, `forward_map`, `direction_repeat` are pure and stay |
| `src/app/pad_owner.*`, `focus_follow.*`, `game_screen.*` | Claims over grabbed pads, Gamescope focus, overlay modes | Shell window raised and lowered; Guide heard in the background through `SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS` |
| `src/steam/client.*` (Managed, Isolated), `sandbox.*`, `secret_bus.*`, `orphaned_steam.*` | `systemd-run` scopes, bwrap, `xdg-dbus-proxy` | None; Steam mode is Normal only |
| `src/steam/desktop_steam.*`, `library/steam.cpp` roots | `~/.steam/steam.pid`, `~/.local/share/Steam` and the Flatpak root | Windows: `HKCU\Software\Valve\Steam\SteamPath`; macOS: `~/Library/Application Support/Steam`. `steam://rungameid/<id>` to launch |
| `src/steam/devtools.*`, `downloads.*`, `install_wizard.*`, `uninstaller.*`, `libraries.*` | Nothing in the code; needs Steam's CEF debugging port, which Steam offers on every desktop OS | Kept behind Normal mode, verified per OS before the rows show |
| `src/library/wine.*`, `app/wine_settings.*`, `gog_install_job.*` (Windows build under Wine) | Wine and Proton runners and prefixes | Windows: the Windows build runs natively, no Wine. macOS: off (Wine on macOS is a non-goal); GOG and Epic list macOS builds where the store has them |
| `src/library/epic.*`, `gog*.cpp`, `app/epic_install_job.*`, `sign_in.*` | `xdg-open` for the browser; legendary and gogdl found on `PATH` | legendary and gogdl run on Windows and macOS; the browser through `SDL_OpenURL`. Android: off |
| `src/library/emulators.*`, `emulator_release.*`, `downloaded_emulators.*`, `appimage_runtime.*`, `assets/emulators.json` | AppImage, Flatpak and `PATH` launchers, libfuse2 check | Catalog launchers gain per-OS entries: Windows `.exe` (zip release assets), macOS `.app` in `/Applications`, Android package + activity templates (iiSU's `||`-split command templates, `launch.md` 2.1). AppImage download and `appimage_runtime` stay Linux |
| `src/library/core_target.hpp`, `libretro_cores.*` (`CoreSearch`) | `nightly/linux/x86_64/latest/` and `.so` (now `coreTarget`, per OS/arch in CMake); RetroArch's Linux core folders | Per OS/arch base: `windows/x86_64` (`.dll`), `apple/osx/arm64` (`.dylib`), `android/latest/arm64-v8a` (and `x86_64` for the AVD), whose cores are named `<core>_libretro_android.so`. `info.zip` and `shader_pack.*` are platform-neutral |
| `src/retro/shared_library.*` (core, librashader) | `librashader.so` | `SDL_LoadObject`/`SDL_LoadFunction` everywhere since step 1 |
| `cmake/Librashader.cmake` | cargo for the host target, staged as `.so` | cargo targets `x86_64-pc-windows-msvc` (`.dll`), `aarch64-apple-darwin` (`.dylib`), `aarch64-linux-android` and `x86_64-linux-android` (NDK linker) |
| `src/retro/presenter.*`, `vulkan_device.*`, `hardware_render.*` | Nothing Linux in the code; Vulkan loader from the system | Windows: driver Vulkan. macOS: MoltenVK bundled, `VK_KHR_portability_enumeration` and the portability subset at instance and device creation; GPU cores checked one by one. Android: system Vulkan, the runner's API version and features checked on named devices |
| gpu2d `src/render/gpu_window.cpp` | `X11/Xlib.h` for the translucent visual; `SDL_CreateGPUDevice(SPIRV, "vulkan")` | Translucency through `SDL_WINDOW_TRANSPARENT` where the OS has it, the X11 visual only on X11. Windows and Android stay Vulkan/SPIR-V. macOS: SDL GPU's Apple backend is Metal, so gpu2d also ships MSL from the same GLSL (`gpu2d_compile_shaders` adds spirv-cross or SDL_shadercross output) and picks the driver per OS. A gpu2d change, in its own repo |
| `src/config/config.*` | `/proc/self/exe`, `HOME`, `XDG_*`, `XDG_RUNTIME_DIR`, `DISPLAY`, session bus | See the location resolver in 3 |
| `src/library/gog_token.*` (`writeOwnerOnly`), `settings/install_folders.*`, `app/retro_session.cpp`, `retro/control_server.cpp` | `unistd.h`: `chmod 0600`, `access(W_OK)`, `getpid` | `std::filesystem::permissions` plus Lucent's `ensure_user_data_directory`; writability by a probe write; the own pid from the `ProcessLauncher` backend (`GetCurrentProcessId` on Windows) |
| `src/library/switch_crypto.*` | Nothing; needs libcrypto | OpenSSL 3 from the platform's dependency set (vcpkg, android prefix) |
| `src/net/web_client.*` | Nothing; needs libcurl and a CA store | Windows: Schannel. macOS: SecureTransport/system curl. Android: OpenSSL with the system's CA files as one in-memory bundle (`net::TrustStore`, `CURLOPT_CAINFO_BLOB`); their names use OpenSSL's old subject hash, so `CURLOPT_CAPATH` finds none |
| `extension/opensu-signin/` | Nothing | Firefox and Zen on Windows and macOS; none on Android (sign-in off there) |
| Lucent | Log, config, `lucent::http::Server` already build on Windows (`http_socket.h` has a Winsock path) and macOS; `platform::user_data_directory` already resolves APPDATA, Application Support, XDG and the Android override | Reused as is; the cache, data and runtime dirs are added beside it (title-neutral, so Lucent) |

Already builds anywhere in principle (no OS calls): `vdf`, `zip`, `fileio`, `settings`, `input`,
`ui` (all of it, painters included), `library` models (`catalog`, `shelf`, `library_query`,
`titles`, `rom_titles`, `romset_*`, `arcade_names`, `switch_*`, `core_bios`, `shader_pack`),
`artwork`, `plugin` (Lua), `audio/effect`, `sound_clip`, `sdl_sound_player`, the `retro/` owners
other than the two loaders, `app/` controllers that only talk to those. Each must still be shown
compiling with the platform's compiler; "no OS calls" is a reading, not a build.

## 2. "Acts as a launcher": what each platform keeps

| Feature | Linux | Windows | macOS | Android |
| --- | --- | --- | --- | --- |
| Home grid, Library, search, details, settings, sounds, mouse and keyboard | yes | yes | yes | yes, plus touch |
| Artwork (Steam CDN, gamesdb, libretro-thumbnails, iiSU's APK packs) | yes | yes | yes | yes |
| Switch titles and icons from the player's keys | yes | yes | yes | yes |
| ROMs, `opensu-retro` with shaders, filters and the setting layers, save states from the quick menu | yes | yes | yes | yes (runner in its own Activity) |
| Built-in cores, BIOS rows, slang pack downloads (Downloads page) | yes | yes | yes | yes |
| Standalone emulators | AppImage download, PATH, Flatpak | found and launched; release download where the project ships a Windows zip | found in `/Applications` and launched | launched by intent; installed from the store by the player |
| Steam | Normal, Managed, Isolated | Normal (the player's client), `steam://` launch, DevTools rows once verified | Normal, as Windows | off |
| Epic (legendary), GOG (gogdl) | yes, Windows builds under Wine | yes, native builds | yes, macOS builds where offered | off |
| Lua plugins | yes | yes | yes | yes (read paths under SAF are a later decision) |
| Button mappings for another program's pads (virtual pads, `ForwardMap`) | yes | off | off | off |
| Button mappings for built-in cores | yes | yes, applied inside the runner | yes | yes |
| Gamescope (nested, session, FSR, frame generation, Alt+F4), login session, Isolated Steam | yes | off | off | off |
| Power list, Bluetooth, brightness tabs | yes | off | off | off |
| Volume and output in the quick menu | yes | when its backend lands | when its backend lands | system volume keys only |
| Owned instances, Close game from the pad | systemd scope | Job Object | process group | not owned; Close game is the OS's |

Seams, one owner each, chosen at build time in the owner's own CMake (one source file per backend,
no `#ifdef` in callers):

- `launch::ProcessLauncher` (from `instance.*`): `start(LaunchSpec) -> Instance`, `stop`, `exitStatus`.
  Backends `systemd_scope` (today's code moved, Linux), `job_object` (Windows), `process_group`
  (macOS), `intent` (Android, through the android-port JNI bridge). `command.*` gets the same
  split for short-lived children (`posix_child`, `win32_child`).
- `launch::GameWindows` already exists: add `win32_windows`, `quartz_windows`; on Android the
  handoff's "shown" is our Activity's `onPause`.
- `gamepad::PadSource`: `EvdevPads` (today's `Pads`, Linux) and `SdlPads` (every other OS, and the
  runner's fallback). `PadOwner` asks a `Claims` capability (`canGrab`, `canForward`); the SDL
  backend answers no and the claim states collapse to Free and Away.
- `app::HostServices` already is "the backends for this machine": it builds the Linux set or an
  absent set; Devices and the quick menu show only the tabs whose service exists.
- `session::Presentation` (what `game_screen.*` asks for): `GamescopeOverlay` on Linux,
  `RaisedWindow` elsewhere (lower on launch, raise on Guide or exit).
- `steam::ClientModes`: which Steam modes this build offers; `integrations.*`'s one `inForce` rule
  takes it, so Managed and Isolated cannot be saved where they do not exist.
- `library::Emulators` reads per-OS launchers from the catalog; `libretro_cores` takes its
  buildbot base and library suffix from one `CoreTarget` constant per build.
- `config::Locations` (below) behind `Config`.

UI: every row, tab, integration option and power entry is listed from the capability of the owner
that serves it, never by OS name in a painter. Hidden features get a "no:" row per platform in
project-state's comparison baseline (Gamescope, login session, Isolated and Managed Steam,
external pad remapping, Wine, power and Bluetooth tabs; on Android also Steam, Epic, GOG).

## 3. Per platform

| | Windows | macOS | Android |
| --- | --- | --- | --- |
| Compiler | clang-cl (MSVC STL and Windows SDK): Lucent's CI already builds this way; `CMAKE_CXX_COMPILER_ID` is `Clang`, so `tools/check/cmake.py`'s refusal holds. MinGW-clang would allow cross builds here but costs vcpkg and Rust MSVC-target compatibility; not chosen | Homebrew LLVM Clang for the checked build (as Lucent's CI); a second AppleClang build in the same job keeps it building. The release package comes from one of them, decided in step 3 | NDK r28 Clang (`~/.local/share/android/ndk/28.2.13676358`, the android-port pin) |
| Native deps | vcpkg manifest with a pinned baseline and a ClangCL triplet: SDL3, SDL3_ttf, SDL3_image, curl, OpenSSL, libwebp, libvorbis, zlib, nanosvg, Vulkan headers | vcpkg, same manifest (Homebrew is not pinned and its dylibs need relinking); MoltenVK from the Vulkan SDK release by checksum | `shared/android-port` prefix (SDL3, SDL3_ttf, SDL3_image pinned already), extended with curl, OpenSSL, libwebp, libvorbis |
| Rust | `x86_64-pc-windows-msvc` | `aarch64-apple-darwin` | `aarch64-linux-android`, `x86_64-linux-android` |
| Package | Per-user Inno Setup installer (no admin) installing `opensu.exe`, `opensu-retro.exe`, DLLs, `share/opensu`; Start-menu entry | `openSU.app` (both binaries in `Contents/MacOS`, assets in `Resources`, MoltenVK and `librashader.dylib` in `Frameworks`), zipped; arm64 only unless the player asks for Intel | Signed APK for the GitHub Release; AAB only if Play is chosen |
| Signing | Authenticode needs a certificate that does not exist; unsigned means a SmartScreen warning. Options for the player: unsigned, or a paid service (Azure Trusted Signing) | Ad-hoc `codesign -s -` is the floor (arm64 requires a signature); Developer ID and notarisation need an Apple Developer account. Unsigned/ad-hoc means right-click Open on first run | Release keystore generated by the agent and stored as secrets |
| Secrets | none yet | none yet | none yet |
| User data (one resolver) | config and data `%APPDATA%\openSU`, cache `%LOCALAPPDATA%\openSU\cache`, runtime `%LOCALAPPDATA%\Temp\openSU` | config and data `~/Library/Application Support/openSU`, cache `~/Library/Caches/openSU`, runtime `$TMPDIR/openSU` | config and data the Activity's `filesDir`, cache `cacheDir`, runtime `cacheDir/run`, through `lucent::platform::set_user_data_directory` |

`gh secret list -R SomeoneIsWorking/opensu` holds only `OPENSU_APPIMAGE_SIGNING_KEY`. To add:
`OPENSU_ANDROID_KEYSTORE` (base64), `OPENSU_ANDROID_KEYSTORE_PASSWORD`, `OPENSU_ANDROID_KEY_ALIAS`,
`OPENSU_ANDROID_KEY_PASSWORD` (agent-generated; the private key also needs an off-GitHub backup the
player keeps, since losing it ends updates for every installed copy); optional
`OPENSU_MACOS_CERT_P12`, `_PASSWORD`, `OPENSU_NOTARY_KEY_ID`, `_ISSUER`, `_KEY` and
`OPENSU_WINDOWS_SIGNING_*` only if the player buys those identities.

Location resolver: `config::Locations` is the one owner of config, data, cache and runtime
directories per OS, built on Lucent's `platform` (extended there with cache and runtime, since it
is title-neutral). `Config` stops reading `XDG_*` directly; on Linux the values stay exactly
today's (`$XDG_CONFIG_HOME/opensu` and so on), so nothing moves for existing players. The
executable and assets come from `SDL_GetBasePath` instead of `/proc/self/exe` (it resolves the
AppImage mount and `.app/Contents/Resources` alike); Android assets are extracted once per version
from the APK into `filesDir/share/opensu` so the filesystem readers stay unchanged.

CI (`ci.yml` and `release.yml`, still `v*` tags and `workflow_dispatch` only, actions by SHA,
downloads checksummed, explicit timeouts):

- `windows`: `windows-2025`, clang-cl, vcpkg binary cache optional, `tools.check all
  --platform windows` (build, clang-tidy, clang-format, ctest), then `tools.package windows`.
  Hidden-window tests need a Vulkan ICD on a GPU-less runner: Mesa lavapipe from a pinned
  mesa-dist-win release installed for the whole job, the way `xvfb-run` wraps the Linux job.
- `macos`: `macos-15` (arm64), LLVM and AppleClang builds, ctest, `.app` zip. Whether hosted
  runners expose Metal to MoltenVK is unverified; if not, the job builds, lints and runs the
  GPU-free tests and says so.
- `android`: `ubuntu-24.04`, the android-port contract (pinned Gradle wrapper and checksum, AGP,
  JDK, NDK), `assembleRelease` signed from the secrets, `bundleRelease` if Play is chosen, APK
  inspection.
- `release.yml`'s publish job takes every package and its `.sha256`.

Tooling: `tools/check` gains `--platform`; packaging moves into one modular `tools/package/` with
`appimage` (moved from `tools/appimage` in step 2), `windows`, `macos`, `android` beside it, sharing
`tools/download.py`.

## 4. Android

- Shell: iiSU is an Android launcher (`screens.md`: `MainActivity` singleTask, landscape), so the
  dp layout, Android em typeface sizes and interface scale already describe it; landscape-locked
  as iiSU. Phone (about 800x360 dp) is below the shell's 960x600 window minimum: the minimum
  becomes a desktop-only window constraint and each page is checked at phone and tablet sizes for
  overlap (frames from the AVD and the phone).
- Input: gamepads through SDL (`SdlPads`); touch through `shared/android-port`'s raw contacts:
  the shell's own pointer router already maps click to focus + A and handles the dock, pages and
  breadcrumbs, so touch is tap = pointer click, swipe = page, long press = context menu. The
  runner gets the authored touch layer the android skill requires (multi-touch pad, pause,
  safe areas, hidden while a controller is connected), through the same action policy as pads.
- ROMs: Storage Access Framework tree URIs picked by the player (one per ROM root), persisted
  permissions. The library scan reads through a `RomTree` seam (filesystem on desktop, SAF
  `DocumentsContract` children on Android); the runner gets a file descriptor or a staged copy
  for cores that need a path (android-port's SAF ZIP staging and resumable copies).
- Cores: buildbot `android/latest/arm64-v8a` into app-private storage and `dlopen`ed from there,
  which Android allows (W^X forbids `exec`, not `dlopen` of app-private files). Google Play's
  policy forbids downloading executable code, so a Play build would ship cores in the APK or as
  feature modules; the GitHub APK downloads them as on desktop.
- Runner: `opensu-retro` cannot be `exec`ed from app storage. It becomes a second Activity in
  `android:process=":retro"` with the same `RunnerApp` entry (SDL main), started by the intent
  backend; `retro_session`'s loopback control channel works unchanged across the two processes.
- Standalone emulators: launch by intent from per-console templates in `emulators.json`
  (package, activity, extras, `||` fallbacks, `%PACKAGE%`), as iiSU does (`launch.md` 2.1,
  `MainActivity.B`, `FLAG_ACTIVITY_NEW_TASK`); a ROM goes over as a `content://` URI with
  `FLAG_GRANT_READ_URI_PERMISSION`. Emulators that only take file paths need the template's path
  form and are listed as such.
- Volume: the media stream through AudioManager, the stream games and the volume keys share.
- Off: Steam, Epic, GOG, sign-in extension, the power tab, Lua read paths outside SAF roots until
  decided.

## 5. Build order

Each step lands and is pushed on its own; "shown working" is a run of the real product on that OS.

1. **Platform seams, Linux unchanged.** `ProcessLauncher`, `PadSource`, `Presentation`,
   `ClientModes`, `Locations`, `CoreTarget`, `SDL_LoadObject`, battery on `SDL_GetPowerInfo`,
   capability-listed UI rows; X11, Gamescope, session and host targets only added on Linux.
   gpu2d: X11 visual only under X11. Shown working: on this machine, `opensu` launches a Steam game,
   a ROM on a built-in core and an AppImage emulator with Gamescope nested, frames and `/state`
   identical in owned-instance and pad-claim behaviour; the AppImage builds and smokes.
   Project-state's baseline drops "macOS is not planned" and lists the per-OS deltas.
2. **Windows launcher + CI.** clang-cl build, vcpkg, `job_object`, `win32_child`,
   `win32_windows`, `SdlPads`, Steam registry roots and `steam://`, legendary and gogdl native,
   buildbot `windows/x86_64`, librashader `.dll`, Inno installer, `windows` CI job. Shown working:
   a Windows 11 guest under KVM on this host (none exists: `tools/vm` only provisions Fedora 44
   KDE; a Windows evaluation image is a new `tools/vm` target, no GPU passthrough, so Vulkan is
   lavapipe or virtio-gpu and only correctness is shown) runs the installed build: grid, a ROM on
   mGBA with a shader preset, Close game from the pad. A Steam launch needs Steam installed and
   signed in inside the guest, which the player has to do once.
3. **macOS.** Metal path in gpu2d, MoltenVK in the runner, `process_group`, `quartz_windows`,
   `.app`, `macos` CI job. Shown working: no Mac and no legal macOS VM exist here, so CI can only
   build and run GPU-free tests; a real run needs the player's Mac (or a hosted runner proven to
   expose Metal). Until then macOS is `partial` with that gap named.
4. **Android.** android-port Activity, assets extraction, `intent` backend, `:retro` Activity,
   SAF `RomTree`, touch layer, arm64 and x86_64 cores, signed APK, `android` CI job. Shown working:
   the `codex_shared_api35` AVD (x86_64, under the android-port device lock) for the shell, a ROM
   from a SAF folder on an x86_64 core and an intent launch; then the player's phone over `adb`
   for arm64 cores and performance (frame-time percentiles, thermals), asking before driving it.

## Decisions

- Windows and macOS signing: decided 2026-10-22, the player: "ad-hoc signed is fine". macOS ships ad-hoc (`codesign -s -`); Windows ships without Authenticode (Windows has no ad-hoc signature), so first runs show SmartScreen and Gatekeeper warnings. No paid identities.
- macOS: arm64 only (decided 2026-10-22). Intel Macs can be added later as a universal build.
- Android: the GitHub APK first, downloading cores like the desktop (decided 2026-10-22). A Play build later delivers cores the way RetroArch's Play version does, as Play Feature Delivery modules, since Play forbids downloading executable code.
- Android release key: generated by the agent, stored as repo secrets, and a copy kept outside any repo for the player to back up.

- Release cadence: decided 2026-10-28, the player: "I want regular Windows/Linux/macOS/Android releases, by regular I mean after update milestones". A milestone is a batch of player-visible features or fixes landed and shown working; each gets a minor version tag (`v0.N.0`), and `release.yml` publishes every platform whose build step has landed. A platform whose step has not landed is missing from that release and its notes say so. v0.2.0 is the first milestone release (Linux only).

## Step 1 as built

Landed in the working tree, Linux behaviour unchanged (evidence in project-state S042):
`launch::ProcessLauncher` with `systemd_scope` and `unavailable` backends; `gamepad::PadSource`
with `EvdevPads` and `SdlPads` (`PadSource::grabs()` is the claim capability: false collapses
claims to Free and Away; forwarding needs no separate flag yet since only evdev has virtual pads);
`app::GameOverlay` (the plan's `session::Presentation`: Gamescope on Linux, none elsewhere; the
raised-window behaviour is step 2's); `steam::clientModes()` taken by `Integrations::inForce` and
`modesOffered`; `config::Locations` on Lucent's new data, cache and runtime resolvers;
`library::coreTarget`; `retro::SharedLibrary`; battery on `SDL_GetPowerInfo`; gpu2d's
`window_platform` seam (gpu2d `b0379eb`, Lucent `1a212bc`, pinned in `tools/check/pins.py`).
`launch/command.*` runs every short-lived child through `child_process.hpp`'s backend:
`posix_child.cpp` (the previous code) or `win32_child.cpp` (`CreateProcessW` in a Job Object,
pipes, a quoted command line, `.exe`/`.com` lookup); `launch::ProcessId` and `ownProcessId()` come
from the same backend and replace `pid_t` and `getpid` outside the POSIX backends.
`app::ShellSession` (`shell_session_gamescope.cpp` on Linux, `shell_session_none.cpp` elsewhere) is
the one owner of the session: `main.cpp` asks `runInOwnSession` for the compositor and login
session, `ShellApp` asks `openShellSession` for the game's windows, the focus source and Steam's
window hiding. The window-system-neutral parts moved to `src/display` (`Output`, `displayOutput`,
`OverlayMode`, `FocusHolder`, `holderOf`, `FocusSource`), so `src/session`, `tests/session` and X11
are added on Linux only and a Windows configure succeeds.
`src/host` (logind, BlueZ, backlight, session mode, KDE's night light) is added on Linux only:
`HostServices::detect` is `host_services_linux.cpp` there and `host_services_none.cpp` (every
service null) elsewhere; the Devices screen leaves out its Bluetooth tab and opens on Controllers
without a Bluetooth service, the Guide's Power page leaves out Sleep, Restart and Shut down without
a power service and the session rows without session mode, and the night light reads the desktop
through `app::DesktopNight` (`desktop_night_kde.cpp`, none elsewhere). The platform-neutral parts
moved out: `src/secrets` (the secret store; libsecret looked for on Linux only) and
`fileio::InstanceLock` (`flock`, or an unshared handle on Windows).
`config::Config::executable` is openSU's executable in `SDL_GetBasePath` (with the target's
executable suffix), not `/proc/self/exe`; the assets, the Gamescope fork, `opensu-retro` and
librashader are found beside it as before. `CoreSearch::standard` is `core_search_linux.cpp`
(RetroArch's and the system's Linux core folders) or `core_search_none.cpp` (openSU's downloads only).
`device::ControllerBatteries` is `controller_batteries_sysfs.cpp` on Linux and
`controller_batteries_none.cpp` (no controller battery) elsewhere.

POSIX-only calls moved behind owners with a Windows backend: `clock::localTime` and
`localeClock24Hour` (`src/clock`), `fileio::writeOwnerOnly` and `mayUseFolder`, `app::crashName`
(a signal, or an exception code), and the process table under `launch::ProcessTree` (/proc on
Linux, none elsewhere yet). `secrets::wipeBytes` is the one wipe (`OPENSSL_cleanse`), which
`explicit_bzero` and the Switch keys' own copy were. nlohmann/json is a `find_package` dependency
(it came from `/usr/include` unnamed).

Windows compile check from Linux: `cmake/toolchains/windows-clang-cl.cmake` (clang-cl, lld-link, the
Windows SDK and CRT from an `xwin splat` tree when the host is not Windows), `vcpkg.json` (pinned
baseline) and `packaging/windows/triplets/x64-windows-clangcl.cmake` (release-only, through the same
toolchain). Every manifest port builds that way except OpenSSL, whose Windows port needs `nmake`.

State of the Windows build (clang-cl, local cross-compile, 2026-10-28): every translation unit of
`opensu` and `opensu-retro` compiles, and both link, against a stand-in OpenSSL (the Linux headers
and an import library made from `libcrypto.num`, outside the repo), with
`-DOPENSU_BUILD_LIBRASHADER=OFF`. Neither has been run. Left for step 2: OpenSSL from vcpkg on a
Windows host (its port needs `nmake`); librashader's cargo build for `x86_64-pc-windows-msvc`;
the targets' `-Wall -Wextra -Wpedantic`, which clang-cl reads as `-Weverything` (needs `/W4` under
the MSVC front end); a Windows `ProcessLauncher` and process table; the tests (POSIX-only, not
configured for Windows); the installer and the release job.



## Step 2 as built

Every Windows backend is one source file picked in its owner's CMakeLists; Linux keeps its code.

- `launch::ProcessLauncher`: `process_launcher_job_object.cpp`. The program starts suspended, joins
  a Job Object with `KILL_ON_JOB_CLOSE`, then resumes, so nothing it starts escapes the job. The
  environment block is openSU's own with the launch's variables set over it (sorted, names compared
  without case). `stop` posts `WM_CLOSE` to every top-level window of the job's processes, waits
  5 s (systemd's TimeoutStopSec), then terminates the job. `running` is the job's active process
  count; `startedRunning` and `exitStatus` the started process's handle and exit code.
- `launch::table`: `process_table_toolhelp.cpp` (Toolhelp32 snapshots; each command line from
  `NtQueryInformationProcess` class 60, split as `CommandLineToArgvW` splits it).
- `launch::GameWindows`: `Win32Windows` (a visible, unowned, non-tool top-level window of any of the
  game's processes). `launch/win32.*` holds the RAII `Handle`, UTF-8/16 text and command-line quoting
  that `win32_child.cpp` and these share.
- Steam: `library::standardRoots` and `processHint` (`steam_host_win32.cpp`: `HKCU\Software\Valve\Steam`
  `SteamPath`, then `C:\Program Files (x86)\Steam`; a running game is a process whose command line
  holds `\common\<installdir>\`), `steam::DesktopSteam` (`desktop_steam_registry.cpp`: the registry's
  `ActiveProcess\pid` naming a live `steam.exe`), and `launch::requestSteamRun`
  (`steam_request_url.cpp`: `steam://rungameid/<id>` through `ShellExecuteExW`; Linux keeps
  `steam -applaunch` in `steam_request_applaunch.cpp`). Steam stays in Normal mode.
- `library::windowsBuildsNative`: GOG, Epic and own `.exe` games run as they are, without the Wine
  runner; legendary and gogdl are found on `PATH` as `.exe`.
- `app::GameOverlay`: `game_overlay_raised_window.cpp`. Launch minimizes the shell's window; the
  Guide raises it and its close lowers it again; the game's end raises it. No game-time keys.
- `app::ShellSession`: `shell_session_win32.cpp` (the shell on the desktop, game windows from
  `Win32Windows`, no Steam window hiding, `coversMonitor` false so the window keeps its size).
- Controller batteries: `PadSource::battery`. `SdlPads` reads `SDL_GetGamepadPowerInfo` (and asks SDL
  for pad events while unfocused, since the shell is minimized under a game); `EvdevPads` keeps sysfs.
- Build: `opensu_warnings` gives clang-cl `/W4`; `opensu_windowed` makes both executables GUI
  programs entered at `main`; librashader builds as `librashader.dll` with cargo's
  `x86_64-pc-windows-msvc` target, clang-cl and lld-link (from Linux too, against xwin);
  `vcpkg.json` adds the host tools `pkgconf` and `shaderc` (glslc).
- Tests: every portable test builds and runs on Windows; the ones whose fakes are shell scripts or
  POSIX calls are left out there, and `tests/launch/win32_launch_test.cpp` covers the Windows
  launch backends with real processes (cmd.exe, a job owning a grandchild, the process table).
- Tooling: `tools.check all --platform windows` on a Windows host (vcpkg at the manifest's baseline,
  Visual Studio's environment, clang-tidy, ctest on lavapipe from a pinned mesa-dist-win);
  `tools/package` holds `appimage` (moved from `tools/appimage`) and `windows` (stage the default
  install component, Inno Setup's `packaging/windows/opensu.iss` per user, `.sha256`, then a silent
  install, `--help` and a `--render` frame). `ci.yml` and `release.yml` have a `windows` job on
  `windows-2025`; the release publishes the installer beside the AppImage, unsigned.
- Defects the Windows tests found in shared code: `library::identify`'s folder depth compared
  iterators of two temporary paths (MSVC's STL looped forever); the emulator-pick migration
  replaced `settings.json` while still reading it (Windows refuses); plugin read paths were checked
  against `/h`-style probes, which are not absolute on Windows, so every token path was refused.
- Not linted on Windows: clang-tidy under the MSVC STL sees its throws (a mutex lock in a
  destructor, `bugprone-exception-escape`, about 150 places) and flags by-value sinks
  (`performance-unnecessary-value-param`) across the shared sources, which the Linux tree passes.
  The Windows-only sources pass apart from that. Proper fix: work those findings off, then lint
  the Windows tree in its job.
- Under Wine (local check only): `NtQueryInformationProcess` class 60 is not implemented (Wine
  11.20), so the process table reads no command lines there; the Job Object launch, its exit code,
  environment, output file, grandchild ownership and stop pass.
- Defects the installed build showed: with no `HOME` (Windows sets `USERPROFILE`) the home folder
  was empty and every path under it relative (`config::read`); Add a game offered Lutris on
  Windows (`add_game_offer_windows.cpp`). SDL's own log lines now go through Lucent
  (`app::SdlLog`), and the CI smoke's `--render` failed because the Vulkan loader ignores
  `VK_DRIVER_FILES` in an elevated process, as a runner's are: `tools/check/windows.py` registers
  lavapipe under `HKLM\SOFTWARE\Khronos\Vulkan\Drivers` there. With that, the smoke's
  `--render` gets past driver selection and then dies with `0xC0000374` (heap corruption) before
  the shell logs its device (CI run 40800451380); where is not known. Next: run the smoke under
  the SDK's `cdb` on the runner for the faulting stack.
- Shown under Wine 11.20 (the CI installer installed silently per user into a scratch prefix,
  `--hidden` in a headless Gamescope, frames from the control channel's `/frame.png`): the
  first-run chooser, Home (empty slots: no store integration on), the Guide menu, Settings, Library
  with the consoles of the ROMs found, a game's details, Mesen downloaded from the buildbot's
  `windows/x86_64` folder, and `opensu-retro.exe` drawing the game when run directly. Started from
  the shell, the runner ran in its job but the shell stayed on "Starting" for 45 s, and a launched
  game's output goes nowhere on Windows (no counterpart of the scope's journal), so why is not
  known yet.

## Step 4 as built

The shell runs on Android as `libmain.so` under android-port's SDL Activity; the APK is built by
`uv run --frozen python -m tools.package.android build` (pinned android-port checked out by commit,
its shared native prefix reused or built, both ABIs, Gradle 9.4.1 with AGP 9.2, minSdk 28 for
`posix_spawn` and the Vulkan loader) and signed with the release key from the
`OPENSU_ANDROID_*` secrets (certificate SHA-256
`BDC838D582DDC695CDAB431D2053B636EB93A0C2E1C3F468DD002D1011E43238`, checked by the tool).
`packaging/android` holds the Gradle project, manifest, icon and Java: `ShellActivity` (installs the
assets from the APK into `filesDir/share/opensu` once per install, then SDL), `RetroActivity`
(`opensu-retro` as `libopensu-retro.so` in `android:process=":retro"`, its arguments and
environment from the intent, the process ended when the runner returns), `Launches` (the runner,
or another app by component or am-style action/data/extras with read permission on `content://`
data) and `MusicVolume`. Native owners added: `config::installLayout` (beside the executable, or
the APK's native library dir and `filesDir`), `locations_android.cpp` (`filesDir`, `cacheDir`),
the `intent` `ProcessLauncher` backend over `android::ActivityCall` (`src/android`), the
AudioManager volume backend, `clock::sinceEpoch` (the NDK's libc++ has no `clock_cast`),
`library::cliStoresRun` and an empty `steam::clientModes` (no Steam, Epic or GOG on Android), and
Lucent's `text::from_chars_real` (no floating `from_chars` in the NDK's libc++). The OS's
on-screen keyboard is off (openSU draws its own). HTTPS trusts the system's CA files, read once
into memory (`net::TrustStore`); cores come from the buildbot's `android/latest/<abi>` folder as
`<core>_libretro_android.so`. `Launches` counts the runner as starting until its `:retro`
process appears (it is forked after `startActivity` returns), and the runner sets a core's video,
audio and input callbacks after `retro_init`, as RetroArch does (Mesen keeps its video callback
in an object `retro_init` makes). CI: the `android` job in `ci.yml` and
`release.yml` builds, signs and uploads the APK; `release.yml` publishes it.

Shown on the `codex_shared_api35` AVD (x86_64, landscape 1920x1080 at 420 dpi, phone-sized): first
run's integrations chooser offering only standalone emulators and Lua plugins, the Home grid, the
Guide menu with the media volume, Settings. "Add a ROM folder" opens Android's folder picker (`SystemFolderPicker`,
`ACTION_OPEN_DOCUMENT_TREE`) and keeps a persistable read grant; the folder is saved as its tree
URI and the ROM scan walks it through `DocumentsContract` (`RomTree`, `DocumentTree`); the
runner gets the game copied into `cacheDir/staged` (cores open games by path). A standalone
emulator's launcher can name its Android activity (`"android": "package/class"`, its `args` the
intent's options); it is installed when its package has a launcher activity
(`installedAndroidApps`), and the game's `content://` URI goes to it with a read grant.
`emulators.json` has melonDS (`-d {rom}`) and RetroArch with Mesen, Snes9x and mGBA
(`RetroActivityFuture` with `ROM`, `LIBRETRO` and `CONFIGFILE` extras). Shown: melonDS 2.0.1
(installed on the AVD from its GitHub release, SHA-256 checked) found for a DS game in the picked
folder and opened by intent with the game's URI; melonDS-android only plays games in its own ROM
folders, so the player adds the same folder there (it answered "Could not find ROM"). Unverified:
the RetroArch templates (no RetroArch on the AVD), and RetroArch reading a `content://` game.
Shown: the picker,
the NES test ROM listed from `/sdcard/ROMs` picked through it, Mesen downloaded for x86_64 and
the ROM running on the `:retro` runner. Left for this step: the player's phone (arm64 cores,
performance).

A tablet-shaped display (1920x1600) lays out without overlap. Fixed since: a
refused path shows on the chooser card under the field (`PathEditor::refusal`) instead of a toast
over the card's prompts, and Settings offers the night light only where the platform can warm the
screen (`nightLightSupported`: Gamescope on Linux). Console photos in WebP decode: android-port's
SDL3_image is built with the prefix's libwebp named by path (the NDK toolchain searches only its
sysroot, so `find_library` missed it and SDL3_image dropped WebP silently) and fails without it;
the profiles ask for the `image_webp` capability, which checks SDL3_image's installed
`Findwebp.cmake`. Library on a phone in landscape: a shelf shorter than a screen spreads across it
in fewer rows (`HomeLayout`, every platform; a deliberate departure from iiSU, project-state). Settings names a
picked ROM folder by its display name ("ROMs", `RomTree::label`), not its tree URI. Android has no Add a game tile (`library::addGameOffer` is empty there). The toast is sized in dp from the
interface theme, so it reads the same on a dense phone screen.

Back from a game: the runner and the shell are Activities in tasks of their own (`RetroActivity`
has its own `taskAffinity` and stays out of Recents). Back, a pad's Guide or the touch pause
button in the runner asks for the shell's menu (`shell_handoff::openMenu`, `RetroActivity.openShellMenu`):
the shell comes to the front and, finding the game running, opens its Guide menu
(`game_overlay_activity_task.cpp` reads the request as Guide), whose save, load, reset and options
go over `retro_session`'s loopback as on the desktop. The runner keeps running its loop in the
background (SDL's block-on-pause off, Back trapped) without running the core, so it answers them;
its Vulkan surface is lost with the window and made again on return. Resume sends the runner's task
back to the front (`Launches.resume`); Close game removes its task and process (`Launches.stop`) and
brings the shell forward (`Launches.showShell`), as the Windows overlay raises the shell when a game
ends. The shell cannot draw over the runner, so its menu shows over its own backdrop, not the game.
The runner draws its own touch controls while no pad is connected (`TouchLayer`, art in
`assets/touch/`, through `OverlayPass` over the frame): sized in dp inside the safe area,
multi-touch, its buttons held on the joypad as one more pad. The swapchain is drawn upright on a
turned display and the system turns it (frames came out turned a quarter before). Shown on the
AVD at 1920x1264 (400 dpi) and 1920x1600 (320 dpi): the controls laid out, three fingers holding
right, A and R at once, Back and the pause button opening the shell's Guide menu while the game
waits, a state saved from it, Resume returning to the game, Close game returning to the Library
with the runner's process and task gone. Unproven: hiding the controls when a pad connects and
a pad's Guide (no pad on the AVD).

librashader ships in the APK as `librashader.so` beside the runner for both ABIs, built by cargo
for the ABI's Rust target with the NDK's API-level clang (`cmake/Librashader.cmake`), where the
Android install layout already looks. Shown on the AVD (x86_64): the shader pack downloaded from
Settings › Emulators, `crt › crt-1tap` set for NES, and the runner logging "librashader ABI 2 API
5 loaded" and "shader preset …/crt/crt-1tap.slangp built" with the scanlines on the game's frame.
The arm64 build is packaged but not run.
