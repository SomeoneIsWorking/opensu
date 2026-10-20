# Project state

## Comparison baseline

The baseline is what this machine does today with no openSU installed: Steam's
own Big Picture (`steam -gamepadui`) launched under Gamescope, switched to by
hand from a Fedora KDE Plasma session. That UI lists Steam only. Epic needs
Legendary or Heroic in a separate window, GOG needs Heroic (or gogdl and a script), and emulators and
ROMs have no home at all.

Visible deltas from the baseline:

- Only ROMs and the libretro runner are on out of the box; Steam, Epic, GOG, standalone emulators
  and Lua plugins are each opted into, from a first-run chooser or Settings › Integrations (S039).
  Steam Big Picture has no such choice. Steam can be the player's own desktop Steam (Normal) or
  openSU's own (Managed, the only mode in the login session).
- Epic games (Legendary) and the player's GOG library (openSU's own sign-in) sit in the
  same grid as Steam; emulator ROMs sit behind one tile per console in the Library section, and
  each store and the combined "All games" library behind a tile of its own there.
- A dock (Home, Library) switches sections with L1/R1; Library has Standard, XMB and Carousel
  layouts, chosen with START and kept in the config dir.
- Delta from iiSU: the dock's Home and Library icons (and the XMB's section icon) are openSU's own
  drawings, a house and a controller, shipped as SVG, not iiSU's nav drawables.
- Each launcher's state is a small logo with a status dot, inline in the top-right status pill
  with the clock and battery (a divider between them), where iiSU has its notification bell (openSU
  has no notifications, so the bell and its R2 glyph are gone). iiSU's pill is the bell column then
  `clock | NN% battery`; the launchers take the bell's column, drawn by the one status pill
  painter, without a circle of their own. A badge is a pointer target: hover lights it, a click
  selects that store's tile in Library.
- Delta from iiSU: the status pill is sized to what it shows (launcher column, the measured
  `clock | NN% battery` row, padding at each end), not iiSU's fixed width, so a PC without a
  battery gets a pill that fits its clock. It stays right-aligned.
- A details page replaces the direct launch: A (Enter, left click) on a game opens it, as do Y and
  the context menu's Details. It shows the cover (a Steam hero as a faded backdrop), title, store or
  system badge, installed state, last played and play time, and Play or Install, Hide or Show,
  Update (an installed store game its store has a newer build of, S036), Uninstall (an installed
  store game, two presses), Back as pointer targets (the Install flow is the
  launch panel's). A ROM's page names its emulator and
  whether it is installed, and a button steps through the emulators for its system; the pick is
  kept per game in `settings.json` (`emulators`). iiSU launches on A with no page (launch.md 3,
  `handleRomActivated`) and only shows game details on a dual-display device's second screen
  (`n42.a`, screens.md 3.6); it has no per-game emulator choice (a console's command template picks
  the emulator). openSU also keeps an emulator per system, and a ROM page whose emulator is
  missing offers "Download <emulator>" for one with an official AppImage, with progress, and Play
  works on the same page once it is in. iiSU installs no emulators. The size on disk is not shown because no store source records it.
- A breadcrumb trail in the top bar's start (`Library > GOG > Search "had" > Installed > <game>`)
  names where the player is; each place in it is a pointer target that goes back to that level.
  iiSU names no place.
- The dock stays shown on Library by default: iiSU hides it there 1.2 s after L1/R1
  (`persistentNavBarOnPlatforms` false), which left a mouse or keyboard player no visible way back
  to Home. The Library layout picker has iiSU's "Pin navigation bar" option (here "on Library"),
  default on, kept in `settings.json` as `pinLibraryDock`; off gives iiSU's behaviour.
- Library tools, openSU's own (iiSU has none of the filters or the sort). Global Search is iiSU's
  panel (Ctrl+F, `/`, or a row of the START options): case- and accent-insensitive substring on the
  title, prefix matches first, narrowing as typed; a keyboard types straight into the field, a pad
  gets an on-screen keyboard. The START options (Home and Library) add a sort (recently played,
  name, store), a source filter (a store or a ROM system), installed only and hidden games only;
  sort, source and installed are kept in `settings.json`, the search and the hidden filter are not.
  Hide/Unhide is in the context menu (Select on a pad; Tab, the Menu key or Shift+F10 on a keyboard;
  right click on a tile) and on the details page,
  with Launch or Install and Details; hidden games leave Home, Library, folders, All games and
  search except under the hidden filter. The last-played time of an Epic, GOG or ROM launch is
  recorded by openSU. Library's icon size is iiSU's slider (levels 1 to 20, default 9) in the
  options for all three layouts. START also opens the options on Home now, and Y opens the
  details page.
- The corner prompts name only what works: Back inside a folder, Options (the context menu, whose
  key cap is Tab with the keyboard last used) with a tile focused, Details (A) on a game or Select
  on a folder, Menu on Library, Back and Select on the details page. iiSU shows them all always.
- The title pill names the focused tile on Home too; iiSU shows it only inside sections.
- A signed-out GOG or Epic tile opens that store's sign-in page in the default browser.
- No store client window is ever opened to reach a game.
- Settings screen (START options, last row "Settings"): iiSU's settings dialog structure with ten
  categories (Appearance, Library, Emulators, Steam libraries, Audio, Controls, Guide chords,
  Install folders, Plugins, About), own controller, model and painter. iiSU's Performance, Updates
  and Integrations sections have no counterpart; Emulators (download, update, remove official
  AppImages, emulator per system), Steam libraries, Guide chords and Plugins are openSU's own.
- Pad chords: iiSU has three fixed single-button shortcuts and fixed L2/R2 overlays; openSU lets
  any button be held for a chord (Guide + A quick menu, Guide + Up/Down/Left volume, game actions on
  Settings › Guide chords), keeps a chord's buttons from the game, and still passes a quick tap of
  the held button to it. Holding the button half a second lists its chords (iiSU has no hold;
  SteamOS's held STEAM button is the model), over a game without taking its input.
- Button mappings: iiSU has only a global Swap A/B and X/Y; openSU maps every pad button and adds
  game chords, globally, per emulator (shared by its games) and per game, edited from a game's
  details page (Controls) and Settings › Controls, with a Steam Input switch that hands a game's
  controls to Steam.
- Switch games show the play time Eden recorded (the bundled Eden plugin), with no launch from
  openSU needed; iiSU shows no emulator play time.
- No volume overlay in iiSU (its main activity hands the volume keys to Android); openSU draws its
  own pill on every volume change. No display-scale setting in iiSU (it follows the Android
  density); openSU has "Interface size", 70 to 150 percent, clamped so every screen fits.
- The grid is ours, so layout, tile sizes and page count are ours.
- Steam's Big Picture does not open inside openSU's session: Guide in the shell never reaches
  Steam, and Big Picture opened any other way is closed and the shell put back in front.
- Shipped as a Linux x86_64 AppImage with the Gamescope fork inside, or built from source. Linux
  only: Windows is a possible future target, without Gamescope (nested mode, Alt+F4 handling and the
  login session are Gamescope features with no Windows counterpart); macOS is not planned.

## Current focus

S004 — a game actually reaching its own window. The interactive shell now runs
and is driven, and the launch handoff has been exercised against Steam, but no
game has been observed running yet.

## Capability inventory

| ID | Capability or outcome | State | Factual dependency | Goals |
| --- | --- | --- | --- | --- |
| S001 | Library model and catalog aggregate over every source | verified | — | G001 |
| S002 | Steam library source: manifests, install state, artwork, playtime, favourites | verified | — | G001 |
| S003 | Gamepad input from evdev, hot-plugged, with iiSU's repeat | partial | — | G003 |
| S004 | Launch handoff into a running game, with the shell returning | partial | S002 | G001, G003 |
| S005 | iiSU home grid: Standard (Flow) and WiiSu (Paged) modes, top bar, prompts | partial | S001 | G002 |
| S012 | Loopback control channel: state, injected input, frame capture | verified | S001 | G003 |
| S006 | Epic source via Legendary: owned titles (`list --json`), install state (`list-installed --json`) | verified | — | G001 |
| S007 | GOG source: openSU's own sign-in, token, owned-games listing, installs through gogdl | partial | S018 | G001 |
| S018 | Store sign-in from the player's browser via the opensu-signin extension (GOG, Epic) | partial | S012 | G001 |
| S008 | ROM source with per-system emulator launch, found without configuration | verified | — | G001 |
| S009 | Haptic rumble | missing | S003 | G003 |
| S010 | Own login session entry on Gamescope | partial | S013 | G004 |
| S013 | openSU runs in a nested Gamescope inside KDE at the output's resolution | partial | S004 | G004 |
| S014 | Alt+F4 closes the game in nested mode while Alt+Tab stays with KDE | partial | S013 | G004 |
| S015 | Shell owns every instance it starts and can force-close it from the pad | partial | S004 | G003 |
| S017 | Per-game render resolution with Gamescope FSR upscaling, Deck-style | missing | — | G004 |
| S016 | Steam client started and owned by openSU, with its state in the top bar | partial | S015 | G003, G004 |
| S011 | Steam's own components kept out of the game grid | partial | S002 | G001 |
| S019 | iiSU's UI sounds on the events openSU shares with iiSU | partial | S005 | G002 |
| S020 | Settings screen with every user-meaningful preference, mouse, keyboard and pad | partial | S005 | G002 |
| S021 | Install folders: one default plus Steam, Epic and GOG overrides from one resolver | partial | S020 | G001 |
| S022 | PC volume and mute, volume overlay, remappable shortcut chords (game actions, Guide chords page, chords kept from the game), interface scale | partial | S020 | G002, G003 |
| S023 | Guide menu (Home, Library, Devices, Downloads, Settings, power button) over the home screen and a running game | partial | S020 | G002, G003 |
| S024 | Devices page: Bluetooth, controllers, audio output, display | partial | S023 | G002, G003 |
| S025 | Quick menu on Guide + A over the home screen and a running game | partial | S024 | G002, G003 |
| S026 | Emulator catalog, official AppImage download, update and removal, emulator per system | partial | S008, S020 | G001 |
| S027 | Steam libraries page: list, add, make default, remove an empty library | partial | S020, S016 | G001 |
| S028 | Sandboxed Lua plugins (API v1: play-time sources, emulator game lists), Settings › Plugins, bundled Eden play time | partial | S008, S020 | G001 |
| S029 | CI: pinned sources, Clang build, clang-tidy, clang-format, ruff, ctest on each release tag and on demand; local gates before every commit | partial | — | — |
| S030 | Linux AppImage release, signed, tested on clean Fedora 44 and Ubuntu 24.04 | partial | S029 | G004 |
| S031 | Built-in emulators: libretro cores found or downloaded from the buildbot, updated, removed, started on `opensu-retro`; an arcade zip's core picked by its romset | partial | S026 | G001 |
| S032 | `opensu-retro`: a libretro core run in its own SDL window drawn with Vulkan, pads, keyboard, sound, save RAM, save states and reset from the quick menu, crash and load-failure toasts | partial | S003 | G001 |
| S033 | Button mappings per game and per emulator (shared by its games), with game chords (`docs/issues/per-game-button-mappings.md`) | partial | S003, S022 | G003 |
| S034 | Holding a chord modifier shows its chords, over a game too (`docs/issues/chord-hold-overlay.md`) | partial | S022 | G003 |
| S035 | Downloads page: Steam queue, store installs and emulator downloads with progress and cancel (`docs/issues/downloads-page.md`) | partial | S016, S026 | G001 |
| S036 | Game updates for Steam, Epic and GOG (`docs/issues/game-updates.md`) | partial | S035 | G001 |
| S037 | Uninstalling Steam, Epic and GOG games (`docs/issues/uninstall-games.md`) | partial | S006, S007, S016 | G001 |
| S038 | Windows GOG and Epic games in one shared Wine prefix (own prefix per game on request), Wine runner setting, Steam left to Steam (`docs/issues/windows-games-wine-prefix.md`) | partial | S006, S007, S020 | G001 |
| S039 | Opt-in integrations (Steam, Epic, GOG, standalone emulators, Lua plugins) with a first-run chooser; Steam Off, Normal or Managed (`docs/issues/integrations-opt-in.md`) | partial | S016, S020 | G001, G004 |

## Capability details

### S002 — Steam library source

Reads app manifests across every library folder, install state from both
`StateFlags` and the filesystem, artwork from `librarycache` in the modern (hashed per-app folders) and
legacy layouts, and playtime and favourites from every user profile.

Verified against this machine and against a synthetic install: one installed
game, one known-but-absent game, one Steam component, artwork in both layouts, a
user profile, and a second library folder listed both directly and as library
"0". It reads **37 titles with artwork for 30**.

Three real defects were found and fixed by that test rather than by inspection:
the install root was read twice because Steam lists it as library "0"; a stale
library folder won over a mounted one because content-id de-duplication ran before
the folders were checked for existence; and an app appearing under both `Apps`
and `Favorites` produced two records, so the later one won and every favourited
game's playtime was zeroed.

### S019 — UI sounds

The twelve iiSU effects that have an openSU event are fetched by range from the pinned APK's
`assets/` into `<cache>/artwork/sound/` (about 1 MB, CRC checked, same worker as the cards) and
played on SDL audio, one stream per effect (`audio::SdlSoundPlayer`); a `--hidden` run opens no
device. Always on; there is no volume or
mute setting. Events (`input-sound.md` 3.4): Navigation on a D-pad focus move in the grid and the
Guide menu; EnterConsolesApps on A opening a console, launcher or All games; ExitConsolesApps on B
back to Library; OpenAppRom on a game launch; Open / Close on the install and licence panels
appearing and being dismissed; OpenContextMenu / Close on the Guide menu. Enter and Exit are dropped
when the same effect fired under 91 ms ago. X, Y and toasts are silent. L1/R1 switch the dock's section and play the Domino cue for the
destination's visible tile count (`audio::dominoFor`: 1, 2, 3-5, 6-11, 12+ tiles; the five
OGGs are fetched with the rest). Nothing plays while a game
runs except the Guide menu's sounds. Without an audio device the player logs one warning and is
silent. Verified: the fetch against the real APK (all seven valid 44.1 kHz WAVs) and the headless
render; unverified: audible playback on a device, and iiSU's 4-stream cap (a repeat of one effect
restarts it).

### S005 — Home grid

Rebuilt from iiSU's decompiled code (`reference/iisu/home-grid.md`, `motion.md`,
`input-sound.md`), not from screenshots. `OPENSU_HOME_MODE` picks the mode:

- `standard` (default) is iiSU's Flow grid: 3 rows, column-major, square cells,
  scrolling horizontally with iiSU's lead margin and eased scroll (`hx2.g`,
  `hx2.L`, `hx2.P`).
- `wiisu` is the Paged grid: whole 3-row pages sized by `zj2`, the next page
  peeking past a 36 dp gap, page changes instant, and the page pill (`wf7`).

Owners: `HomeLayout` (geometry, pure), `GridFocus` (neighbour search with
remembered row and column, then iiSU's pixel pass over every page's frames, which
is how focus crosses pages; pure), `tile_motion` (focus scale, domino entrance,
pulse, ring rotation, scroll easing; all time-based, pure), `TilePainter` (shadow,
sweep focus ring, glass chrome, cover-cropped art, platform frame, fallback
letter, in `tx2`/`tw2` draw order, always the dark variant as iiSU's home config
fixes `darkHeroScrim`) and `PagePillPainter` (dark variant). The shell composes
them under `Hud`: iiSU's single-screen top bar (`TopBarMetrics` for is7/hs7/a32.o
sizes, `StatusPillPainter` for the clock and battery, `TopBarLayout` for the pill and the launcher cells it paints and hit-tests at, `ClockText`
for o28.g's format and k42's minute tick), the corner hints and openSU's toast.
The battery comes from `device::BatteryReader` (sysfs, system scope only); the
12/24-hour choice from the LC_TIME locale in `config`. The title pill (`jj2.c`)
names the focused tile everywhere, Home included, where iiSU's Home shows none. There are no feature tiles or badges on game tiles.

The dock (`ui::DockPainter`, metrics `DockMetrics`, motion `DockVisibility`/`IconPop`,
`navigation.md` 1.2-1.5, 4) is a bottom-centre glass capsule with Home and Library icons and LB/RB badges, in the
variant matching the chrome (light). The icons are openSU's own SVGs (`assets/icons/home*.svg`,
`library*.svg`: a house and a controller, gradient body, white face, extruded edge; the active
section's `_selected` variant has a brighter gradient and a glowing face), shipped in the repo and
rasterised by `ui::IconAtlas` at the size each rests at, so every install, CI and the README draw
them sharp at any UI scale. It is shown always on Home, and on Library while pinned (default) or else for 1200 ms after
L1/R1 or a pointer resting where it is, with a 250 ms tween; it is not focusable. L1/R1 used to turn grid pages; they now cycle
`library::Sections` (Home, Library, wrapping), each section remembering its focus. Page turns
are D-pad only. The slide follows the dock capture (navigation.md 5.1): straight down while fading
(FastOutLinearIn, 125 ms), back up from below (LinearOutSlowIn, 170 ms). The glass blurs the scene
behind it with a real 8 dp Gaussian (`BackdropBlur`). Gap: the inner rim highlight is not drawn
(its clip is unrecovered).

Library's modes (`ui::section_view`, `rail_layout`, `rail_painter`; navigation.md 5): Standard is
3 rows x 4 columns, column-major, paged horizontally ("Horizontal Mode"); the earlier single-row
render came from stale persisted settings in the capture driver, not the layout. XMB: slots start at
340.5 px of 1920, focused 434 px, others 217, gap 17; the left column shows the section icon
(openSU's `library.svg`, its gradient and edge swapped for the focused console's border colours) and a marker, and inside a console a 72 dp card
for the console; the focused title sits right of the card (cap 25.8 dp, #4D4655 light, soft shadow).
Carousel: focused item at 0.5 W (also the first), shared bottom edge at 918/1080, title centred at
cap top 364/1080 (cap 12.9 dp), a marker under the focused game; the entrance fades the focused
tile in 130 ms and its neighbours 170 ms later. Console cards sit 2 dp inside their slot, game cards
fill it; only Standard draws the cyan-violet focus ring. START opens the chooser (`ModeChooser`);
the choice is saved by `settings::Store`. Home is always the grid. Placeholders (empty glass cells, four pages minimum) are Home's: the ROMs
category level and folders in captures draw only their items, so `HomeLayoutInput::fillSlots` is true
only for Home. That rule is read from `roms_standard_light_categories.png`; the RE has no ROMs-specific
branch (`ou4.q` builds placeholders for every Grid caller). Its left inset equals Home's (8 dp edge
inset, measured the same in both captures). Page dots and the right arrow draw in WiiSu (Paged) mode
as in `home_dock_light.png`, which is a WiiSu capture; Standard mode has none, as in
`home_standard_light_initial.png`. Deviation: openSU opens the
cards page directly from START; iiSU goes START, "Customize Platforms", header arrow, cards. The
chooser lacks iiSU's options list below the cards. The title shadow numbers are a `// guess:`.

Home's shelf (`library::homeShelf`) is the store games installed here, a title installed in
several stores once. Library's shelf (`library::libraryShelf`) is, in order: one launcher tile per store whose catalog
source is not absent (Steam, Epic, GOG: the badges' presence rule; a signed-out store keeps its
tile, captioned "Sign in", and A on it opens the store's sign-in page in the default browser off the loop, `ShellApp::startSignIn`); an "All games" tile when any
store has games; one console tile per system with ROMs, in `rom_systems` order; 
Uninstalled store games are only inside the library pages, so Home does not carry hundreds of
titles it cannot launch.
A console tile is
iiSU's own card for it when the starter pack has one (below), drawn as the whole
tile, cover-fit and clipped at the content radius: the card carries the glyph and
its frame, so the tile has no name or count. Without a card (not downloaded yet,
offline, or a system the pack lacks, such as PS4) it keeps openSU's own tile: the
platform's gradient (a neutral one for a system the gradient table lacks) with the
console's name and game count, broken onto two lines at a space when one is too
wide, at one name size so the typeface loads one face. A launcher tile is the same
card in the store's brand colours with its Simple Icons logo above the name and count;
the All games tile is a violet card with a four-square library glyph. A opens a console,
a launcher or All games (`library::ShelfBrowser`, `library::Folder`) on its games; B returns to
Library with that tile focused. A launcher page holds the store's whole library, installed or
not, in catalog order; All games holds every store game once. Titles are merged across
stores by normalised title (`library::titles`: ASCII letters and digits lower-cased, "&" as
"and", so "Hades" and "HADES™" are one; a title with no ASCII letters is never merged; a
store never merges two of its own games). The entry is the preferred copy: installed first,
then Steam, GOG, Epic. A store game tile (Home, All games) carries small store icons in its
bottom-right corner for every store that owns it (`Game::ownedIn`,
`ui::storeIconRow`); on a launcher page only the other stores are shown, and a game owned
there alone shows none. ROM, console and launcher tiles never carry them. Inside any of the
three the title pill names the focused game. A store game with an update (S036) carries a down
arrow on the store icons' disc in its top-right corner (`ui::updateBadge`), opensu's own.
Nothing a large library does runs on the frame thread (measured on the 447-title Epic
library: `legendary list --json` 2.3 s to 10 s; cover decode 113 ms and upload 16 ms per Epic
image at full size, all of a shelf's tiles in one frame):

- Stores list on threads of their own (`library::CatalogLoader`, one per provider); the
  loop takes each listing as it arrives and rebuilds the catalog. A store whose first listing is
  not in is `Availability::Loading`: its Library tile reads "Loading", its top-bar badge spins,
  and A on it says it is loading rather than opening sign-in.
- Covers decode on a worker (`ui::ImageDecoder`), scaled to at most 768 px on the long side,
  and the frame uploads at most `Shell::uploadsPerFrame` (4) textures. Only tiles the layout
  owner places near the canvas (`HomeLayout::visibleSlots`, the XMB's and Carousel's window)
  are decoded; textures are released oldest first outside that window once `Shell::residentLimit`
  (120) are held, and re-decoded from the file when the tile comes back. Layout, hit-testing,
  painting and artwork requests cost the same for 50 tiles and 5000 (unit-tested). Per-event
  work that is still O(items): D-pad focus search (`FocusGrid::of`).
- A game tile whose art is queued or downloading by the fetcher (`ArtworkFetcher::stageOf`),
  or whose file is decoding, shows a rotating arc in the focus ring's cyan; a confirmed miss or
  an unreachable round settles it (`Fetched::saved == false`) and it keeps the first-letter
  placeholder. iiSU has no tile spinner (`launch.md` F), so the arc is openSU's own, drawn with
  the Material spinner. The fetcher takes the games on screen first.
- Epic key image URLs with spaces are percent-encoded; one such URL used to end the whole round.

A tile loads its artwork when it comes near the canvas, so a shelf change or a download needs
no separate load step. A shelf holds copies of the catalog's games made before later downloads, so every rebuild
(`ShellApp::showShelf`) takes each game's file from `ArtworkStore::apply(shelf)`.

Artwork that no source has on disk is downloaded in the background
(`artwork::ArtworkFetcher`) into `<cache>/artwork` (`$XDG_CACHE_HOME/opensu`):
a Steam game's `library_600x900.jpg`, else `header.jpg`, from Steam's CDN; an Epic
game's key image from `Game::artworkUrl` (`epic::Provider` takes it from `legendary list
--json` `metadata.keyImages`: `DieselGameBoxTall`, else `DieselGameBox`, else `Thumbnail`),
kept as `epic/<app name>.jpg`; a GOG game's portrait cover, the `game.vertical_cover.url_format`
of gamesdb's `platforms/gog/external_releases/<product id>` (public, no token) with
`_glx_vertical_cover` and `jpg` filled in (342x482), else the library's `Game::artworkUrl` stem plus
`_196.jpg`, kept as `gog/<product id>.jpg`; a ROM's
box art from libretro-thumbnails, its file or folder name matched against the
system's `Named_Boxarts` listing (same name, else same title preferring the ROM's
own region, then USA, World, Europe, Japan; never a beta or demo; release numbers
and GoodTools region letters understood). A source's "not found" is kept as a
`.miss` for 14 days; an unreachable source ends the round without one. Listings
are kept for 30 days. Measured on this machine: 54 images on the first run, 10
misses (libretro lists only 67 PS3 and 12 Xbox 360 boxes, a PS4 folder named by
title id, two Steam apps without either image). An arcade game's box art is the `MAME` set's,
matched by its listed description. A PS4 dump shows its own `sce_sys/icon0.png` and is titled by
its `sce_sys/param.sfo` TITLE (trademark signs dropped), which is also its box art key. A Switch
game shows its own icon and is titled by its own `control.nacp` (`library::roms::SwitchInfo`,
`docs/issues/switch-art.md`): the folder's base file (title id ending `000`, else the largest
non-update), an NSP's PFS0 or an XCI's secure HFS0 partition, its Control NCA (the base title's
when an XCI also carries the update's) decrypted offline with the player's own keys from Eden's
`keys/prod.keys` and `title.keys` (`$XDG_DATA_HOME/eden/keys`, else Eden's Flatpak), a key the
file lacks derived from the master keys as hactool does. The American English title (else the
first language's) and `icon_AmericanEnglish.dat` (else the first icon) are kept as
`<cache>/artwork/switch/<size>-<mtime>.title` and `.jpg`, and the square icon goes through the
same tile and details cover path as a PS4 `icon0.png`. Without keys the tiles keep their file
names and iiSU's first-letter fallback, with one warning per listing. Verified on this machine:
27 of 27 Switch games under `/mnt/Boy/ROM/Switch` (NSP, trimmed XCI, folders of base, update and
DLC files) titled and shown with their own icons in a hidden run in 0.5 s, then 27 from the cache
on the next start (`scratch/switch-art/frame.png`, `shell-*.log`); the key derivation matches
every derived key in a real prod.keys (header key, 21 key area keys, 21 titlekeks). Not read: an
NSZ/XCZ whose Control NCA is compressed (`.ncz`), and NCA2 or older headers.

A ROM's tile title is `library::roms::cleanTitle` of its file or folder name: everything from
the first `(` or `[` goes (region, language, revision, dump flags, `Decrypted`, `-patched`), a
release number prefix goes, a trailing `, The` (also A, An, Le, La, Les, Il, El, Der, Die, Das,
Los, `L'`) moves to the front, and a name without spaces reads its underscores as spaces. The
artwork key stays the untouched name, so box art matching is unchanged. An arcade system's zip
is a MAME short name, so its title is the description libretro-database lists for it
(`sf2` is "Street Fighter II: The World Warrior (World 910522)"), cleaned the same way, and that
description is its artwork key, the name libretro's MAME box art carries. The
listings are `metadat/fbneo-split/FBNeo - Arcade Games.dat` (1,791,303 bytes, CRC-32 67cc252a)
and `metadat/mame-split/MAME 2016.dat` (2,118,571 bytes, CRC-32 1ecf3c2e) of libretro-database
commit `bf825e3ec48d` (CC-BY-SA-4.0), a short name in both read as FinalBurn Neo has it.
`ArtworkFetcher` downloads them in the background the first time the catalog holds an arcade
ROM and no `<cache>/names/arcade-<commit>.tsv` exists, checks size and CRC-32 against the pin,
and keeps the merged 11,491 short names parsed as that tab-separated file (488 KB). The ROM
provider only reads the file during `list()`, so a catalog read never waits on the network;
until the file exists an arcade game shows its short name, and the shell re-reads the catalog
when the names arrive (`Fetched::Kind::Names`). The pin and the cache file name change together
with the commit. iiSU itself scrapes ScreenScraper/TheGamesDB with per-user credentials and
ships only a Vita title-id table, so there was no iiSU name source to match.
Gaps: a ROM whose file name is a code on a system other than arcade (a Vita `PCSE00905` pkg)
keeps that name; the Vita table (`psvita_title_ids.json` in the APK) is
the candidate. A hash lookup is not done: the listings carry the CRC of the whole zip, which
differs between a user's re-zipped copy and the listing (`armwar.zip` and `avspu.zip` here match
neither), so it would only work for TorrentZipped sets. A game the two listings lack (newer than
MAME 2016 and not in FinalBurn Neo) keeps its short name.

Console cards are `platforms/<system>.webp` (512x512, ES-DE system names) in iiSU's
starter pack, `assets/iiSU_StarterPack.zip` inside the 0.0.7.4 release APK
(`iiSU-Alpha-7.4.apk`, GitHub). `artwork::StarterPack` fetches only that entry with
HTTP Range requests (the APK's tail for its zip end record, the entry's header and
data, 25 MB of the 128 MB APK), checks it against a pin (APK size, entry size and
CRC-32, then the zip's own CRC-32; a 200 answer to a range or any mismatch stores
nothing) and keeps it as `<cache>/artwork/iisu/starter-pack-0.0.7.4.zip`. Each
card is decoded with libwebp and kept as `<cache>/artwork/console/<system>.png`;
a system the pack lacks gets a `.miss` like a game. iiSU's assets are downloaded,
never shipped. The older starter-v1.0.0 release is not used: it lacks GBA, GC,
PSX, PS2, PSP, Switch and Wii.

A ROM tile's platform frame carries its console's white glyph in the tab, as iiSU's
`IconComposition` does (`g24.e`): contain-fitted into the square at 45/1024 and 90/1024 of
the short side (`frameGeometry().glyph`, the tab's centre). The glyph is
`assets/borders/logo_<console>.png` in the same pinned APK, named by
`assets/borders/border_pack.json` (`consoles[] {console, border, logo}`).
`artwork::ConsoleGlyphs` reads that map once through `artwork::ApkArchive` (the one ranged
APK reader the starter pack shares), then takes only the logos of systems with ROMs, each
CRC-32 checked, into `<cache>/artwork/glyph/<system>.png`; a system the pack or APK lacks
gets a `.miss`. The shell reads stored glyphs on each catalog load and takes new ones as they
arrive (`ui::GlyphTextures`). Without a glyph the tab stays empty. Console cards and store
tiles are unchanged. Measured against the real APK: five glyphs (gc, psx, snes, n64, switch)
are 4.3 KB in 8.8 s, over about 11 requests, most of it GitHub's redirect latency.

The status pill's bell column (iiSU `a32.n`; openSU has no notifications) holds the launcher badges
(`ui::LauncherBadgePainter`, mapped in `app::launcherBadges`): Steam, Epic and
GOG, each its Simple Icons logo in an avatar circle at a32.e's avatar size, spaced
rather than overlapped, with a presence dot (green ready, amber starting with a
spinner ring, red failed or blocked). A launcher not installed has no badge; GOG is always
there, red until the player has signed in. Epic's
and GOG's state is the catalog's last read of them (`library::SourceStatus`): a
store whose tool is missing is absent, one that fails to list (Legendary signed
out) needs attention. `/state` publishes them as `launchers`
(`steam=ready epic=failed gog=ready`) and the open shelf as `shelf`. Icons are SVGs
rasterised by nanosvg at the drawn size (`ui::IconAtlas`). Verified headless at
1280x720: badges, console tiles, opening GameCube, the ROM title pill and B back
to the console.

Verified by `tests/ui` (layout numbers hand-computed from `hx2.g` and `zj2`,
neighbour and page-crossing rules, motion curves, tile geometry from `tj2.V`,
top-bar sizes at 640/853/1280 dp, clock formats and battery icon slots),
`tests/device` (battery parsing from a fake sysfs) and by rendering both modes
offscreen at 1280×800 and 1920×1080 and looking at them.

Stopgaps, each marked in code:

- dp is `min(W/853, H/480)`; iiSU's density comes from Android.
- WiiSu's 12 dp horizontal padding is not applied and `zj2` takes the grid gap as
  its spacing; the call site's arguments are unresolved.
- `hx2.z`'s cross-flow fallback for multi-lane tiles is not ported; every home
  tile is 1x1.
- Top bar: glass is the fill only (no border or shadow; the dock's glass has the blur); the `jj2.w` strip
  is not drawn; the status text row
  is centred after the launchers' column; text ink is the icons' `#4D4655`.
- Corner prompt panels: each prompt shows only while its button does something (Back in a
  folder, Details with a game focused, Select on a tile, Menu on Library); their glass is the fill only, like the top bar's.
- The battery is re-read on the clock's minute tick; there is no uevent listener.

Gaps: items are ordered by install state and recency rather than iiSU's user
arrangement; WiiSu placeholders do not take focus as they do in iiSU; held
keyboard directions repeat every frame.

Offscreen rendering is how the layout became checkable at all: the shell is a
Wayland window, which this machine's screenshot tooling cannot see. The shell draws the frame into
an offscreen gpu2d target and encodes it as PNG in memory, so a frame is a file.

Type is Cal Sans 1.000, iiSU's default face (`pp4.a`; the others it offers are Console Sans
and Arctainium), shipped in `assets/` under the OFL with the licence beside it. The file is the
Google Fonts build, which matches the one in iiSU's APK in metrics and Latin-1 outlines. Text
sizes are Android's: the em in pixels, with letter spacing in ems; `TextGrid` sizes the face so its
`hhea` ascent-to-descent line box is whole pixels and cuts each advance to whole pixels, as the
stb_truetype atlas the shell drew from before did, and `Typeface` draws each glyph there through
SDL3_ttf, unhinted.

Gap: the per-platform HSL tinting the reference applies to artwork is not
implemented, so covers are shown at their store's own colours.

### S003 — Gamepad input

`gamepad::Pads` reads every evdev node that is a gamepad (`BTN_SOUTH` and a left
stick), so a keyboard is never one, and watches `/dev/input` with inotify so a pad
switched on later is picked up; it skips openSU's own and Steam Input's virtual pads.
`gamepad::PadTranslator` maps the kernel's gamepad codes to the shell's controls,
the left stick and hat included, so any driver following them works (xpad, xone,
hid-playstation, hid-nintendo). Held directions, from a pad or the keyboard, move
once and then repeat after 400 ms every 100 ms (`gamepad::DirectionRepeat`, iiSU's
95 ms throttle on Android's repeats). Tested: `pads` against uinput pads (hotplug,
Steam's virtual pad ignored, disconnect, a game's mapping on the virtual pad, a motion node never
grabbed), `direction_repeat`, `pad_translator`. A game's button mapping (S033) applies only to
the virtual pad the game reads; the shell's controls are never mapped.

Inside Gamescope the shell holds the pads while its own window is in front (`gamepad::Claim::Shell`,
`app/pad_owner`), so the Steam client openSU runs never sees Guide; a Steam dialog in front gets
them, and the shell then hears only Guide, which brings it back (`app/focus_follow`). Big Picture is
closed whenever it takes the focus. Outside Gamescope the shell leaves the pads free.
`docs/issues/guide-opens-steam-big-picture.md`. Tested: `pads` (Shell, Away, Free against another
reader), `window_focus`, `focus_owner`. Gap: not yet seen on the real pad in the real session.

The GLFW reader this replaced had no mapping for the xone driver's Xbox
controller, so its buttons never registered, and counted a Logitech K400 Plus as a
controller; the keyboard path moved focus on every frame a key was down.

Keyboard and pointer: `input::keyBindings` is the one key table (arrows/WASD, Enter/Space, Esc,
F, Y, Tab = Select, E = Start, `[` `]`, R); `ShellApp::handleKeyboard` reads it and the prompts
name its first key per button. `input::LastDevice` records which device gave the latest real
input (a pad button, a key, a moving or clicking pointer; a still pointer, a pad connecting and
the sticks inside the translator's threshold do not count); while it is the keyboard and mouse,
every prompt (`ButtonGlyphPainter`: corner hints, dock LB/RB, launch panel, Guide menu) draws a
key cap in the ring's size and stroke instead of the controller glyph.

Mouse: `app::PointerRouter` maps one `PointerFrame` onto the shell's actions through
`PointerHost` (`ShellApp`), and `Shell::pointAt` says what is under the pointer from the painters'
own geometry. Hover focuses the tile, layout card or Guide row under a pointer that moved (the
focus the D-pad moves, with the Navigation sound; a still pointer never refocuses); the dock lights
its item as before. Left click is focus plus A on a tile, card or row, the section change on a dock
item, the page turn on a page arrow or dot, and the hint's own button on a launch-panel hint
(install, store choice, licence accept/decline, cancel). Right click is never Back: on a game tile
it opens that game's menu, on a folder or store tile the tile's menu (Open, Sign in, Refresh), on
anything else it does nothing, and outside an open menu it closes it. The wheel
steps the way the pad does: a page in WiiSu, a column in Flow, down the XMB, along the Carousel,
between layout cards, down the Guide menu. Hit-tests: `HomeLayout::slotAt/pageAt`,
`railTileAt`, `ChooserLayout::cardAt/rowAt/iconSizeAt`, `SearchLayout::keyAt/resultAt`, `ContextLayout::itemAt`, `GuideLayout::rowAt`, `QuickLayout::rowAt/levelAt`, `PanelLayout::hintAt`, `dockItemAt`.
Tested: `home_layout`, `rail_layout`, `mode_chooser`, `search_panel`, `context_menu`, `guide_menu`, `quick_menu`, `launch_panel`, `pointer_router`,
plus `last_device`, `keyboard_bindings`, `dock_metrics`, `sections`, `control_channel`.
A launcher badge hovers lit and a click selects that store's tile in Library (`ShellApp::selectLauncher`,
`TopBarLayout::launcherAt`); the options panel's rows, the icon size slider (a click or drag sets the level), the search keys and results, and the context menu's rows hover and click like a card.
Not clickable: the rest of the top bar, the corner hints, the toast, the XMB's header
card (B goes back). The sign-in prompt is the Library launcher tile, so it takes the
tile click. `opensu --render FILE --keyboard` draws the keyboard's prompts; `POST /input` takes `a keyboard`
and `/state` reports `inputDevice`.

Gap: not yet confirmed on the real Xbox controller, or the mouse behaviour above with a real
mouse (the control channel injects buttons only, so hover, click and wheel are covered by unit
tests, not by a driven run).

### S004 — Launch handoff

The game gets its own session so a shell exit or a hangup cannot reach it, and the
shell's window goes down once the game shows a window and comes back when it
leaves. Until then the shell shows a launch panel over the grid: the title, the
stage (waiting for Steam, updating with a bar, Steam's launch task, starting, loading) and B to cancel.
Inside Gamescope "shows a window" means a pid in the game's process trees owns an
entry of `GAMESCOPE_FOCUSABLE_WINDOWS` on the root (window, app id, pid triples,
the pid found by Gamescope); outside it there is no display to watch and a running
process counts. A game that leaves before it shows a window is reported as such.
Verified in a headless Gamescope with an emulator that sleeps 6 s before opening
glxgears: the panel showed Loading and the shell hid at 7 s.
Hiding is a request to the main loop, not a call from the handoff thread: the
window belongs to the loop's thread, and there is no queue that makes its calls
safe from elsewhere. The launch thread only raises a flag. Every source records a `processHint` — the
Wine prefix path for Steam, the install folder for the others — which is what
identifies the game in the process table.

Waiting is two phases: the game must appear, then it must leave. Neither is a wait
on the child: `steam.sh` lives for hours and a launcher that hands off exits at once.

For a Steam game the handoff follows Steam's own launch (`steam/launch_activity.*`):
a recorder in Steam's SharedJSContext keeps each app's latest game action (id,
task, error, ended) and its running state from `GameSessions`. The launch's action
is the one whose id is newer than the id seen before `-applaunch`. While that action
is unfinished the panel shows Steam's task ("Synchronizing cloud", "Running
first-time setup", ...) and the appearance bound is not counting down. Steam
launches run in stages — Cuphead runs an install script under `reaper --verb=run`
for 10 s, idles 2 s, then starts the game — so the game is only "gone" when Steam
says it is not running, no process matches the hint and Steam's action is over.
An error Steam reports on the action (`AppError_N`, localized) is the failure. A
game Steam already runs is returned to without a second `-applaunch`, which would
only raise Steam's "Game already running" dialog. Non-Steam games keep the
process-and-window check, bounded at three minutes.

Verified: Cuphead at 3840x2160 in a headless Gamescope launches through setup to
its window with no false "closed before it showed a window" (the reported bug),
and the shell hides at the window. `handoff_test` covers a staged launch with a
gap, a Steam-reported error and a returned-to running game.

Gap: the user saw Cuphead drawn small in the top-left after opening the Guide
menu (`docs/issues/guide-shrinks-game.md`); not reproduced.

### S007, S018 — GOG library and store sign-in

GOG (`library/gog_auth.*`, `gog_token.*`, `gog.*`): openSU signs in itself, with GOG
Galaxy's own client id and secret as minigalaxy and gogdl do (GOG has no third-party
registration). `Auth::loginUrl` is the page the player signs in on; it ends on
`https://embed.gog.com/on_login_success?origin=client&code=<code>`, and `Auth::signIn`
exchanges the code at `auth.gog.com/token`. The token (access, refresh, expiry, user id)
is one JSON file, `<data dir>/gog-token.json` (`$XDG_DATA_HOME/opensu`, else
`~/.local/share/opensu`), mode 0600 in a 0700 directory, replaced atomically;
`Auth::accessToken` refreshes it a minute before it expires. The library is
`embed.gog.com/account/getFilteredProducts?mediaType=1&page=N` (minigalaxy's call): one
request per page returns id, title and image, so no per-game requests. Games are listed
not installed, with `Game::artworkUrl` set to GOG's image stem (`https:` + `//images-N.gog.com/<hash>`,
to which `_<size>.jpg` is appended; minigalaxy uses `_196.jpg`), the fetcher's fallback tile.
No token, or a refresh GOG refuses, is `Attention` on the GOG badge. Tested against a local
server (`tests/library/gog_test.cpp`); no real GOG account has been used.

GOG installs (`app::GogInstallJob`, a `CliInstallJob` like Epic's) run `gogdl --auth-config-path
<file> download <id> --platform linux|windows --path <data dir>/gog-games/<id>`; gogdl is
Heroic's `heroic-gogdl` 1.3.1, installed with `uv tool install` pinned to a revision (README).
Decisions:

- Token: `TokenStore` stays the only saved token. Before each install the job refreshes it
  through `Auth::accessToken` (openSU's refresh) and writes it for gogdl with
  `gog::GogdlAuthFile` into `<data dir>/gogdl-auth.json` (0600): gogdl keys credentials by
  Galaxy's client id with `access_token`, `refresh_token`, `loginTime` and `expires_in`
  (`gogdl/auth.py`). A download longer than the token's life makes gogdl refresh and rotate the
  token itself, so afterwards the job reads the file back, saves a changed refresh token into
  `TokenStore` and deletes the file.
- Platform: the library listing's `worksOn` (`{"Windows","Mac","Linux"}` per product in
  `getFilteredProducts`) is parsed into `Game::builds`; the install downloads `linux` when GOG
  lists a Linux build, else `windows`, else fails with "GOG lists no Linux or Windows build of
  this game". Epic passes no platform.
- Progress and failure: gogdl logs `[PROGRESS] INFO: = Progress: 12.34 505/4096, ...` (no `%`);
  `library::install_log` parses that and legendary's line, and the ERROR/CRITICAL log lines of
  both; gogdl's `Unable to proceed, ...` line (disk space) is a failure too.
- Installed state: gogdl keeps no list and picks the game folder itself, so on success the job
  records `{path, platform}` in `<data dir>/gog-installs.json` (`gog::InstallRecords`); the
  provider reads it at each listing, marks the game installed and launches it with `gogdl launch
  <path> <id> --platform <p>`; a Windows install gets the Wine runner and prefix at launch
  (S038). Uninstall (S037) deletes the recorded folder only inside the root openSU
  gave gogdl, else only forgets the record. The record also keeps the installed build (S036). No
  repair yet; a game folder deleted by hand stays recorded.

Unverified: no real GOG download was made. gogdl's output format, the folder layout under
`--path` and `gogdl launch` come from reading gogdl 1.3.1's source and a fake gogdl in
`tests/app/install_job_test.cpp`; the `worksOn` field comes from minigalaxy's use of the same
endpoint and is only exercised against the fake GOG server in `tests/library/gog_test.cpp`, so a
real account's answer may differ (a product without it has no builds and cannot be installed).
Windows installs run under the Wine runner picked in Settings › Wine (S038).

Sign-in is a browser flow (`app::StoreSignIn`, driven by the control channel's `/signin`
routes). `open` runs `xdg-open <page>`, so the player's default browser (Zen here) is used
and Flatpak browsers need no special case. The page is GOG's above, or Legendary's
`https://legendary.gl/epiclogin`, which redirects to Epic's login. A WebExtension,
`extension/opensu-signin/` (Manifest V3, Firefox/Zen 142+), watches for the two endings and
POSTs the code to `http://127.0.0.1:7311/signin/<store>`, then closes the tab once openSU
answered 200:

- GOG: `webNavigation` on `embed.gog.com/on_login_success`, the `code` query parameter.
- Epic: Legendary's login ends on `https://www.epicgames.com/id/api/redirect?clientId=...`,
  which answers JSON holding `authorizationCode` (this is what `legendary auth` asks the
  player to paste). The extension reads that response with `webRequest.filterResponseData`
  without changing it. Epic's code goes to `legendary auth --code <code>`; Legendary exits
  0 even when Epic refuses the code, so the Epic badge after the reload is the proof.

Loading it: `about:debugging` → This Firefox → Load Temporary Add-on →
`<datadir>/opensu/opensu-signin.xpi`, which `extension/CMakeLists.txt` packs and installs (gone at
browser restart). A Flatpak browser's portal file picker exposes only the chosen file, so the bare
`manifest.json` loads without `background.js`; the `.xpi` is one file. The port is the constant
`OPENSU_PORT` in `background.js`, openSU's default 7311; edit it when `OPENSU_CONTROL_PORT`
differs. The source and the `.xpi` lint clean (`web-ext lint`, `addons-linter`).

Verified (GOG): user-confirmed real GOG sign-in in Zen 1.23b (Firefox 157) with the extension
sideloaded into the profile, 2026-10-09. Epic's sign-in through the extension is not yet
verified against a live account, so S018 stays partial.

Known gap: the channel does not check `Origin` (`lucent::http::Request::header` now exposes
it), so any local web page can POST a code to it (a login-CSRF, not a credential leak).

### S012 — Control channel

A loopback HTTP channel, part of the product rather than a debug flag, so an
automated run can drive the shell with no controller and no compositor in the way.
`GET /state` returns the shell's state as JSON (`artworkPending`: tiles near the canvas still
showing the loading placeholder, downloading or decoding or not yet uploaded), `POST /input` queues a tap (press and release) of a button by
name (`guide+a` holds buttons together, `guide 1500ms` holds them up to 10000 ms, every later edge
queued behind the release), `GET /frame.png` returns the next frame as PNG bytes, `POST /quit` closes
the shell. `OPENSU_CONTROL_PORT` moves the port; it cannot be closed, because it
is how the shell is driven. It binds loopback only and names no file to read or
write — frames come back as bytes over the response.

Sign-in routes (S018), all `POST`: `/signin/gog/start` and `/signin/epic/start` open the
store's sign-in page in the default browser; `/signin/gog` and `/signin/epic` finish a
sign-in with the authorization code as the body (letters, digits and `-_.~` only; anything
else is a 400). A finished sign-in answers 200 `{"ok":true,"store":...,"message":...}`
and reloads the catalog; a store that refuses the code answers 502. The code is never
echoed or logged. There is no UI button for them yet.

The shell owns the GL context, so a frame request is handed to the main loop over
a condition variable and answered there rather than drawn on the request thread.
Injected buttons go through the same event path a real press takes, so what the
channel exercises is the shell's own handling and not a parallel one.

This is what made the interactive path checkable. Verified: state read, five
rightward moves walking focus across two pages, `start` returning to the first
tile, a frame captured after injection showing the focused tile's ring moved,
and `quit` shutting the shell down. The first launch through it is what exposed
the S004 wait defect.

### S009 — Rumble

Missing: the GLFW reader that had it is gone; evdev force feedback is the way back.

### S010 — Own login session

Partial: everything is built and unit-tested; no real login has been run.

`opensu --session` outside Gamescope runs `session::CompositorSession` with a native
`Output`, which is `gamescope --backend drm -- opensu --session` (the connector's
own mode; no `-W/-H/-w/-h/-f`, which are nested-only, and no `-e`: Steam is started
by openSU as in the nested session). `cmake --install` puts
`<prefix>/share/wayland-sessions/opensu.desktop` (`Exec=<prefix>/bin/opensu
--session`, `DesktopNames=gamescope`, from `packaging/opensu-session.desktop.in`) and the two
root-run scripts in `<prefix>/libexec/opensu/` (`install-session.sh`, `opensu-session-select`).
SDDM here (0.21+) reads `/usr/local/share/wayland-sessions` and `/usr/share/wayland-sessions`
(`sddm --example-config`; `/etc/sddm.conf` sets no `SessionDir`), never `~/.local/share`.

Install session mode (Power list outside the login session, `host::SessionMode`): "Install
session mode" shows while no entry for this openSU is in a directory SDDM reads or the selector is
missing; "Switch to session mode" once both are there. Install opens a dialog (what it is, that
openSU becomes its own login session on Gamescope with no desktop, that Power then Switch to
desktop comes back, what is installed where, that an administrator password is needed; Accept and
Cancel by pad, keyboard or mouse; a right click is never Back). Accept opens a masked password
field with the on-screen keyboard (Caps on X; letters, digits and `- . & ' /` only, the physical
keyboard types anything) or the physical keyboard. The password goes to `sudo -S -k -p ''
/bin/sh <prefix>/libexec/opensu/install-session.sh` on stdin only (never argv, log, settings or
control channel), from a worker; the buffer is wiped as soon as sudo has had it. A wrong password
shows sudo's line and asks again; the installer's first output line (`opensu-install: start`) is
how a sudo refusal is told from an installer failure. Cancel, or B on an empty field, aborts.
The prompt is in openSU because a desktop polkit dialog is invisible inside nested Gamescope.

The one root step installs, as root: `/usr/local/share/wayland-sessions/opensu.desktop`;
`/usr/local/libexec/opensu-session-select` (0755); and `/etc/sudoers.d/zz-opensu-session-select`
(0440, checked with `visudo -cf` before it is moved in, last, so a failure never leaves a rule
without its selector): `<user> ALL=(root) NOPASSWD: /usr/local/libexec/opensu-session-select
opensu, /usr/local/libexec/opensu-session-select restore`. Scope: one user (`SUDO_USER`, validated
`[A-Za-z_][A-Za-z0-9_.-]*`, not root), one root-owned program in a root-owned directory, and only
its two fixed arguments (sudoers matches the argument list exactly); it cannot run anything else.
`sudo -n` can use it without a password the way SteamOS's session switch does.

The selector is POSIX sh, not Python: it runs as root on a machine openSU does not control, needs
no interpreter, module path or environment (`PATH` and `LC_ALL` are set in it, sudo resets the
rest), and is about 150 lines to audit; the project's Python rule is for project tooling, not for a
shipped system helper. It takes exactly `opensu` or `restore`, anything else (none, two, other
words, empty, injection) exits 1 before touching a file, and `SUDO_USER` is only compared, never
written. `opensu`: with SDDM autologin configured for this user, writes
`/etc/sddm.conf.d/zz-opensu-session.conf` (`[Autologin] Session=opensu`; it sorts after
`kde_settings.conf`, so it wins); without autologin, sets SDDM's last session
(`/var/lib/sddm/state.conf`, `[Last] Session=`, the full path of the entry; SDDM's `StateDir`
default) so the login screen preselects openSU. It remembers the previous autologin and last
session once, in `/var/lib/opensu/previous-session`. `restore` removes the drop-in (what is left
is the player's own autologin session again) and puts the remembered last session back only if
state.conf still names openSU's entry. It refuses autologin for another user and a `Session=` in
`/etc/sddm.conf`, which SDDM reads after the drop-ins. `OPENSU_SELECT_ROOT` prefixes every path,
honoured only for a non-root caller (tests); under sudo the variable is dropped.

Switch to session mode: second press, then `sudo -n <selector> opensu`, then
`busctl --user call org.kde.Shutdown /Shutdown org.kde.Shutdown logout` (Plasma 6; the method and
its sibling `org.kde.LogoutPrompt.promptLogout` exist on this machine, read with `busctl --user
introspect`; `logout` skips KDE's own confirmation, which the second press replaces). SDDM then
logs in again to openSU. If the logout call fails the selection is undone with `restore`.
Switch to desktop in the session: `sudo -n <selector> restore`, then stop
`<session>-compositor.scope`; SDDM logs in again to the previous session (here `steam-picker`,
which starts Plasma while `~/.gamemode-session-flag` is absent). Superseded and deleted: the
`steamos-session-select` branch and `PowerAction::SwitchToDesktop` in `host::Power` (stopping the
Gamescope scope does the same without a SteamOS script). Kept: `host::DesktopRequest` and `main`'s
`startplasma-wayland` hand-over, as the fallback when the selector is not installed (a login
session entered through an entry installed by hand, no root helper): it works without root and
needs nothing from SDDM.
Abnormal end (`host::LoginSessionEnd`, run by `session::LoginSession` after the top-level
Gamescope ends): SDDM with `Relogin=true` and autologin on openSU logs in again at every end, so a
failing Gamescope (lavapipe: `Failed to initialize Vulkan`; Venus: abort) was a relogin loop of
about one login per second, and "Switch to session mode" would have trapped a user in it. The end
is abnormal when Gamescope exits non-zero or by a signal, or exits at all within 30 s of the start
(`quickEndWindow`; the inner openSU dying at once is also such an end), and nobody asked for it.
Not abnormal: SIGINT/SIGTERM taken by `CompositorSession` (logout, shutdown; `stopRequested()`),
and a Switch to desktop, which leaves the `DesktopRequest` note after the selector's `restore`
(now with the selector installed as well, not only for the fallback). An abnormal end with the
selector installed runs `sudo -n <selector> restore` (the one `host::runSelector`, shared with
`SessionMode`), so the next login is the previous session and nothing is tried again; a restore
that fails is logged as an error and the exit status stays non-zero. Without the selector openSU
changed no autologin, so it logs that and does nothing. This lives in the login-session path of
`main`, not in the session entry's `Exec`: a wrapper script would be a second implementation of
the same decision, would need the exit status and the stop request that only the process has, and
the entry would stop being `opensu --session`, which `SessionMode::installed()` matches. Residual:
a failure before `main` runs (a missing shared library, the loader) or openSU's own process being
killed by a signal cannot restore anything; the next login is openSU again until Switch to desktop
or a TTY.

Unverified: a real login on hardware, the DRM start of the fork on a real seat, libseat under
SDDM, Steam in the top-level session, `[Last] Session=` form and effect without autologin, `sudo
-S` with a real wrong password on a real machine, `visudo` and SELinux contexts on the user's
machine. Verified in the VM (`docs/vm-harness.md`): the install dialog with a wrong then the right
password, Switch to session mode from Plasma, Switch to desktop, SDDM re-reading the drop-in at
relogin, and the abnormal-end restore against the failing DRM Gamescope (at most one failed openSU
login, then the previous session stays up).
Evidence: unit tests `session_mode`, `session_scripts` (the real scripts against a temp root, the
real `visudo`), `login_session_end` and `login_session` (fake sudo, selector and gamescope),
`session_install_controller`, `session_dialog`, `guide_menu*`, `host_services`; hidden-run
captures under `scratch/session-install/`; VM shots and journals under `scratch/vm/`.
Open VM finding: Plasma ending 10 to 30 s after the `startplasma-wayland` hand-over (selector not
installed) was seen in 2 of 3 early greeter runs and not reproduced since: 3 autologin and 4
greeter hand-overs stayed up for 140 s, as did 3 plain greeter Plasma logins
(`scratch/vm/fallback-*`, `greeter-*`). The hand-over environment is the plain one except the
entry's identity (`DESKTOP_SESSION=opensu`, `XDG_CURRENT_DESKTOP`/`XDG_SESSION_DESKTOP=gamescope`
from `DesktopNames=gamescope`); no `WAYLAND_DISPLAY`, `OPENSU_*` or Gamescope variable, same
session scope and fds (`scratch/vm/env-*.txt`). The early runs overlapped with host load and, in a
later batch, with a second harness loop, so they are not evidence. Whether the fallback stays is
undecided.

Picker integration is not done (it would edit `/usr/bin/steamos-session-picker`, which is the
player's own).

### S013 — Nested Gamescope session

Outside Gamescope (`Config::insideGamescope`, from `GAMESCOPE_WAYLAND_DISPLAY`)
`session::CompositorSession` runs openSU itself as `gamescope -W -H -w -h -r -f --
opensu ...` with the current monitor's size and refresh from SDL's display modes
(`session::readMonitor`), in the scope `<session>-compositor.scope` with
`OPENSU_SESSION` set for the inner opensu. When Gamescope ends it stops every
other scope named `<session>-*`, and SIGINT/SIGTERM (blocked for the whole run; `launch::Instance`
starts every program with no signal blocked) stop the session through its scopes. When Gamescope
ends abnormally (a signal or a non-zero exit) and neither a signal nor Switch to desktop's note
(`DesktopRequest::pending`) asked for it, the session's leftovers are stopped (the game and Steam
die with it) and a fresh Gamescope is started, logged as a warning, at most 3 times in any 2
minutes (`session::RestartBudget`); past that it gives up with an error and returns the crash's
status, which the login session then judges as before. Games are never wrapped in a Gamescope of
their own. The argument vector and the session's run, leftover cleanup, signal path, restart, cap,
no restart on a requested stop or on the way to the desktop (against a fake `gamescope` that
crashes N times) are tested. `uv run --frozen python -m tools.nested_remap.restart` runs the real
`CompositorSession` and the pinned Gamescope in the private headless KWin with a stand-in child
(`tests/session/nested_session_driver`; the real shell would grab the machine's pads), SIGABRTs
Gamescope by the PID in its scope once each child is up: 4 kills gave 3 restarts, each with a new
Gamescope and child, then the give-up and exit 134. Not run: the restart with the real shell,
Steam and a game in the user's KDE session.
The binary is the pinned fork of S014, at `Config::gamescope`; a missing binary (openSU
configured with `OPENSU_BUILD_GAMESCOPE=OFF`) is a one-line error, not a PATH lookup. The
fork is built in podman (S014), so the host needs podman and nothing else.

### S014 — Alt+F4 in nested mode

KWin handles Alt+F4 and Alt+Tab as its own global shortcuts before nested
Gamescope sees them. KWin's Alt+F4 sends Gamescope's window a close, and stock
Gamescope answers a close with `raise(SIGTERM)` on every backend (3.16.29
`WaylandBackend.cpp` `LibDecor_Frame_Close`, `SDLBackend.cpp` 825), so the whole
session ends. KWin can block all global shortcuts for a window (`gamescope
--grab`) but not one, which loses Alt+Tab.

openSU therefore runs a fork, `SomeoneIsWorking/gamescope` branch `opensu`, pinned
to commit `6654c87f20eb212c159e3a22cfbf7ccc03af5dfd` (3.16.29 plus eight commits).
It adds `--close-focused-window`: a host close request sends `WM_DELETE_WINDOW`
(or the xdg close) to Gamescope's focused app window and the session keeps
running. `gamescopeArgs` always passes it. The other three are upstream's
`WaylandBackend` fix series (c50ddfa9, 6867f509, e0602ccc, cherry-picked): with no focus window
Gamescope unmaps its toplevel with a NULL buffer, and the next frame attached a buffer before the
host's new configure, so KWin killed the connection (`xdg_surface` error 3) and the session with it
whenever a game quit to nothing (`docs/issues/nested-gamescope-dies-on-game-quit.md`). The last
three end the session on a dead host connection instead of aborting: upstream PR 2393's
`WaylandBackend` input thread shutdown (b35aaa12, cherry-picked), a failure exit status for that
shutdown (`ShutdownGamescopeOnError`, 883b7842), and the host display's hangup left to the read
that reports it (79db8c0d; `IWaitable::OnPollHangUp` aborted first). 6654c87f includes `float.h`
in `wlserver.cpp` for `DBL_MAX`, which the AppImage's Ubuntu 24.04 build (Clang 22, libstdc++ 14)
does not get through other headers. `cmake/Gamescope.cmake` builds the pin
as an ExternalProject (meson, release, Clang; the layer and the tests are off) whose
configure and build steps run in a rootless podman container built from
`packaging/gamescope-build/Containerfile` (host's Fedora release, `dnf builddep gamescope`;
image tagged by the Containerfile hash), then checks the staged binary with `ldd` on the host
(Fedora hosts only; no host-build fallback; `OPENSU_GAMESCOPE_BUILD_ENV=native` runs meson directly
inside an environment prepared for it, CI's Fedora 44 job container and the AppImage's Ubuntu 24.04
build container) and stages it where `Config::gamescope` finds it, in the build
tree and installed. A pin change fetches the commit into the existing clone
(`cmake/GamescopeFetchPin.cmake`, before the disconnected update that would refuse it) and runs
`meson setup --reconfigure`, so the version header (`git describe`) follows the pin. Verified in a
scratch tree configured at `41e84d4f` with the new pin absent from its clone: the bump fetched,
rebuilt and reported `3.16.29-4-geea763bd`, and the unchanged rebuild after it had no work; the
bump to `79db8c0d` did the same in `build/clang` (`3.16.29-7-g79db8c0d`).

With no game running, the focused app window is openSU's own, so Alt+F4 reaches
SDL as a window close request: `ShellWindow::pollEvents` returns false, which ends the frame loop in
`ShellApp::run`; it stops the control channel and returns, the window closes with `ShellApp`, the inner openSU exits, Gamescope ends with its only client and
`CompositorSession` stops the session's scopes. That is the intended desktop behaviour.

Verified (virtual `kwin_wayland`, a KWin script closing Gamescope's window, an
`xmessage` client): without the option Gamescope exits on the close; with it, on the
Wayland, SDL and xdg paths, the client's window closes and Gamescope stays up, and
a second close with no app window left is ignored. Evidence:
`scratch/gamescope-fork/test/` (`run_case.sh`, `close_gamescope.js`, per-case logs).
The ExternalProject recipe was built in a Fedora 44 image with all dependencies and
a second build compiled nothing. Not verified: a nested session in the real KDE
session, the shell's own window closing there, and Alt+Tab staying with KDE. The
fork is built without Gamescope's Vulkan WSI layer; whether a game needs it
nested is untested.

### S015 — Owned instances

Every scope openSU creates is named `<session>-<role>[-N].scope`
(`Config::session`: `OPENSU_SESSION`, else `opensu-<pid>`), and is a transient
systemd user scope (`launch::Instance`), so processes that setsid or double-fork
stay owned. A non-Steam game runs in its own scope,
stopped when the game leaves. A Steam game is handed to the background client
(`steam -applaunch`). Close game in the Guide menu force-closes either: the scope,
or for Steam the reaper whose command line carries `AppId=<id>` and everything below
it (`launch::ProcessTree`), leaving the client up. Scope ownership, escape-proof
stop, tree kill and force-close are tested with real processes and a fake `steam`;
a real Steam launch is not yet exercised.

### S023 — Guide menu, S024 — Devices page, S025 — quick menu

Guide (a tap on the pad's Guide button, which comes out on release so Guide + A can be a chord, or
Shift+Tab, Steam's overlay key, read by `session::GameKeys` over a game) opens a menu down the
left edge: Home, Library, Devices, Downloads and Settings outside a game; Resume, Close game,
Devices, Downloads and Settings over one. The stores are in Library, not here. Power is a power icon at the left end
of the bottom row, with the A and B hints stacked at its right end: Down past the last row focuses
it, Up returns, a click or A opens the power list
(Sleep, Restart, Shut down, Install session mode or Switch to session mode, Quit to desktop).
Sleep, Restart and Shut down are `systemctl suspend|reboot|poweroff` through `host::Power`;
Restart, Shut down and Switch to session mode need a second A; the session entries are S010. With
`--session` (openSU is the login session) the list ends with Switch to desktop instead of Quit to
desktop, since quitting the login session only makes the display manager start it again; untested
against the real
session, only against fakes. Over a game it draws through the Gamescope overlay (`GameScreen`); the Devices,
Settings and quick pages do too.

Devices tabs: Bluetooth (BlueZ over `busctl`: adapter on/off, scan held by `bluetoothctl`, pair
with trust and connect, connect, disconnect, forget on Select twice or a right click; no
PIN or passkey agent, so Just-Works pairing only), Controllers (name, player order, battery,
live held buttons, link to shortcut remapping), Audio output (volume, mute, default sink through
the volume owner) and Display (interface size, output size and rate, brightness where a backlight
exists; no resolution backend). The quick menu (Guide + A, Ctrl+Tab on a keyboard) holds volume,
mute, output, brightness, Bluetooth, controller batteries, Devices and, over a game, Close game.

A hidden run reads Bluetooth and the backlight and changes nothing, never runs a power command
and never starts Steam (`host_services`, `Arguments::startsSteam`). Tested with fakes:
`host_services`, `bluetooth_session`, `devices_controller`, `quick_menu_controller`,
`guide_menu_controller`, `controller_roster`, `audio_outputs`, `brightness_control`,
`shortcut_router`, `pointer_router`, `guide_menu`, `quick_menu`, `game_keys` (Ctrl+Tab). Verified
headless (`--hidden`, captures under `scratch/guide-menu/` and `scratch/guide-trim/`): Guide menu, power list and armed
restart, Devices tabs reading the real adapter, quick menu, Settings without the title pill, a
hidden Restart refused. Unverified: real power actions, scan, pairing and connect, the overlay over
a real game under Gamescope, mouse clicks on the new pages (routed and unit-tested only), a
physical pad's battery and button test, a backlight (this machine has none).

From launch until the launch ends, `gamepad::Pads` holds every gamepad, including one
connected meanwhile: each
physical pad is grabbed (`EVIOCGRAB`) and the game reads one uinput Xbox 360 pad
(045e:028e) per physical pad instead, translated by `gamepad::PadTranslator` (axes
rescaled to xpad's ranges, a button d-pad as the hat, digital triggers as full
axes). Guide never reaches the game; the shell reads its controls from the same
pads, held or not. A virtual pad is ready only once udev has opened it to the user,
so a game enumerating at once can open it. While
the menu is open the virtual pads rest (held keys released, axes centred); on
resume they take up the held axes, and keys held then stay up until pressed again.
A game openSU starts gets `SDL_GAMECONTROLLER_IGNORE_DEVICES_EXCEPT=0x045e/0x028e`,
so SDL hides the grabbed pads. Tested: `pad_translator` (unit), `pads`
against real uinput pads, `handoff` passing the environment; verified headless with
a uinput pad driving the shell (`scratch/overlay-test/drive_pad.py`): B reaches the
game while playing, Guide and all menu input do not, the virtual pad is gone after.

Gaps: a Steam game gets Steam's environment, not the SDL hint; Steam Input reads
hidraw where a pad has it, which a grab does not cover (an xone pad has none, and
Steam then reads the virtual pad); a non-SDL game that enumerates every evdev pad
sees the grabbed, silent one too.

Gap: a force-close while the client is still starting the game cannot cancel the
request it already handed to Steam.

### Installing Steam games

A on a game that is not installed offers to install it (A installs, B cancels). Steam and
Epic and GOG install. A title owned in
several stores is installed from the stores that can: with one candidate A installs it, with
two stores (say Steam and GOG) the panel reads "Install from" with A for the first store and X for the
second. A launcher page installs its own store's copy; Home and All games choose among
the copies. Launching an entry owned in several stores uses the installed copy, Steam first,
then GOG, then Epic.

`app::InstallJob` is the store-neutral job (thread, latest report, licence answer);
`app::SteamInstallJob` implements it; `app::CliInstallJob` is the base of the two jobs that
drive a downloader program (`app::EpicInstallJob`, `app::GogInstallJob`, the latter described
under S007); `app::Installs` routes by store, one install at a time. The Epic job runs `legendary install <app> -y --skip-sdl`
(`launch::runStreaming`, stdin closed, stderr and stdout merged) and shows the percentage of
legendary's `= Progress:` line (`library::install_log::progress`) on the same panel and
"Installing · N%" line as Steam's; a failure shows the first ERROR/CRITICAL or
" ! Failure:" line. Its output format is legendary 0.20.35's own, from `cli.py` and
`downloader/mp/manager.py`; no game was installed on a real account to capture it live.
The Steam job drives Steam's own installer through `steam::InstallWizard`, the
same `SteamClient.Installs` calls Steam's library makes: OpenInstallWizard, then
ContinueInstall at the config step with Steam's default library folder, until the
wizard hands off. A licence agreement stops the job and the panel asks the player
(A accepts and records it with `SteamClient.Apps.MarkEulaAccepted`, B declines and
cancels the wizard); a CD key, password or sign-up step cancels with a toast. The
download is then followed through the live queue ("Installing · N%" on the panel; B
hides it and the download goes on), and the catalog is reloaded once the manifest
says installed. Measured with the real client on this machine: Spacewar (480) opened
the wizard, reached its EULA (`480_eula_0`) and cancelled cleanly
(`scratch/update-progress/live`). Steam's UI shows its own "Install" popup while the
wizard is open; whether that window takes Gamescope's focus has not been measured.
Epic titles come from Legendary; neither store is signed in on this machine.

### S008 — ROMs

ROM roots are found when `OPENSU_ROM_ROOTS` is unset: a `ROM`/`ROMs`/`roms` folder
in the home folder, `~/Emulation`, or at the top of a drive under `/mnt`,
`/media/<user>` or `/run/media/<user>` that holds a known system's folder. Systems
(`library/rom_systems`) are matched by folder name with only letters and digits
compared ("PSX CHD" is psx, "Wii U" is wiiu); a system folder holds game files or
game folders, and a folder starts its marker (Wii U `code/*.rpx`, PS4 `eboot.bin`,
PS3 `PS3_GAME/USRDIR/EBOOT.BIN`) or else its game file that is no update or DLC.
Emulators (`library/emulators`, from the catalog `assets/emulators.json`) are found
as openSU's downloaded AppImage, on PATH, as AppImages in `~/Applications`,
`~/AppImages`, `~/.local/bin`, or as Flatpaks; a system without one is listed, and
play names what to install or offers the download (S026). On this machine: 124 games in the
grid from `/mnt/Boy/ROM`; Dolphin (GameCube), PCSX2 (PS2) and Eden (Switch)
launched in a headless Gamescope, showed a window in 1.5–2.5 s and closed from the
Guide menu (`scratch/roms-real/smoke.py`). Cemu, RPCS3, shadPS4 and Xenia Canary
arguments come from their `--help` and are not launched yet. Not covered: PS5,
Vita `.pkg` (needs installing into Vita3K), XBLA, Amiga disk sets, Android.

### S016 — Steam client

Steam runs on the desktop's session bus. Its tray icon cannot be suppressed: `-silent` is
"tray mode only", and no tray switch or flag was found in `steam`, `steamui.so` or the UI bundle
(only `MinimizeToSysTray`). On a private `dbus-run-session` bus there is no
StatusNotifierWatcher, so Steam falls back to an XEmbed icon, which KDE's `xembedsniproxy`
re-exports on the desktop's bus as a bare item (Id `Steam`, `WindowId` set, no menu, `ItemIsMenu`
false). That was the icon that showed and ignored left and right clicks. Measured on KDE Plasma
(Wayland) with a real Steam, not signed in: private bus gave `:1.N/StatusNotifierItem` owned by
xembedsniproxy; the session bus gave Steam's own `org/ayatana/NotificationItem/steam` with a
`/Menu` object, and no proxy item. The private bus also left `xdg-desktop-portal-kde` and `ksecretd`
in the scope after Steam exited, so the scope never emptied; on the session bus it empties 5 s
after `-shutdown`. A working icon was chosen over a hidden one; its menu contents and clicks were
not exercised.

A Steam left running when openSU dies without `Client::shutdown` (killed by PID) stays in
`opensu-<pid>-steam.scope`, and its pid file made the next start Blocked. `Client::start` now ends
such scopes first (`steam::OrphanedSteam`): an active `opensu-<pid>-steam.scope` whose `<pid>` is
not a live process of the same program is stopped, through `steam -shutdown` first when the pid file
names a process inside it. Ending rather than adopting: the orphan's flags, bus and log position
are unknown, and `Instance` owns a child it started. A Steam outside openSU is in no such scope, so
it still blocks. Tested with fake scopes of a dead pid and of a live one.

Steam's downloads are read live from the client itself: Steam is started with
`-cef-enable-debugging`, which serves Chrome DevTools on 127.0.0.1:8080, and
`steam::DevTools` evaluates `SteamClient.Downloads` registrations in its
SharedJSContext once a second while Ready (`steam::DownloadQueue`). App manifests are
no use for this: during a measured BTD6 (960090) update at 44 Mbps the manifest's
byte counts and StateFlags stayed unchanged for minutes while the client reported
92% done. The queue drives three things: a launch of a game with an unfinished
download keeps the shell up with `Updating · N%` and no appearance bound (B cancels);
the Steam badge shows a progress ring; and a
progress ring fills around the Steam badge.
Reproduced from a real run: BTD6 with a 2.2 GB update left an empty Gamescope
behind a hidden shell before the launch waited for it.

`steam::Client` starts `steam -silent -cef-enable-debugging` in `<session>-steam.scope`
when openSU starts, if a Steam install exists, and watches it: Initializing until a logon line
(`[Logged On` with `RecvMsgClientLogOnResponse() : processing complete`) is
appended to `$HOME/.steam/steam/logs/connection_log.txt` after the start, Failed when
the scope empties, Blocked when `$HOME/.steam/steam.pid` names a live client outside
opensu. Steam launches wait for Ready, and are refused with a named message when
Blocked or Failed. On exit it runs `steam -shutdown`, waits up to 20 s, then stops
the scope. The state shows as the Steam badge in the top bar's friends slot (S005)
and in `/state` as `steam`. Big Picture is closed through the client whenever it takes Gamescope's focus
(`steam::BigPicture`, `docs/issues/guide-opens-steam-big-picture.md`). Tested with a fake home and fake `steam`; a real Steam
has not been started by this code, and the ready marker is as measured on one
machine.

### S011 — Steam's own components

**Stopgap.** App manifests carry no tool-versus-game marker; that lives in
`appinfo.vdf`, whose v29 layout wraps the app section in a header block that is
not plain KeyValues and was not decoded. Components are therefore matched by the
directory names Steam installs them under, which removed 9 entries from this
machine's grid. The list drifts as Valve adds runtimes, so decoding `appinfo.vdf`
is the real fix.
### S017 — Per-game resolution and FSR

Not built. The installed Gamescope exposes the Deck's runtime controls as root
window atoms: `GAMESCOPE_XWAYLAND_MODE_CONTROL` (switch an Xwayland server's
mode, so a game sees a smaller screen), `GAMESCOPE_SCALING_FILTER` /
`GAMESCOPE_NEW_SCALING_FILTER`, `GAMESCOPE_FSR_SHARPNESS`, plus
`--xwayland-count` for a separate Xwayland per role. Their exact semantics come
from Gamescope's source (`steamcompmgr.cpp`) before any of it is used.

### S020 to S022 — Settings, install folders, volume, chords, scale

Settings: `src/app/settings_controller.*` rows per category, saved through `Preferences` into
`settings.json`; ROM and Steam root changes apply after a restart. Skipped as not user-meaningful
or not safe live: window size, control channel port, emulator command overrides, 24 hour clock
(locale-derived), theme, removing a single ROM or Steam folder.

Install folders: `settings/install_folders` resolves store override, else default, else the store's
own choice. Epic passes `legendary --base-path`, GOG `gogdl --path <folder>/<id>`. Steam targets a
library through `SteamClient.InstallFolder.AddInstallFolder/GetInstallFolders` and
`Installs.SetInstallFolder(index)` in the install wizard, and only into Steam's own libraries.
Verified against the steamui JS and a fake DevTools Steam in tests, not live Steam.

Volume: `audio/volume_backend` (wpctl, pactl, read-only shadow for hidden runs) behind
`audio/system_volume`. openSU applies its own shortcuts; the desktop's media keys are observed by
polling, so one change is one step and one overlay. Missing backend
shows the install command in the Audio page.

Chords: `input/shortcuts` is the one table (defaults Shift+Tab Guide, Ctrl+Up/Down/M volume, Q quit,
Guide+A quick menu, Guide+Up/Down/Left for volume on a pad), remapped on the Controls page with
conflict refusal and saved. Esc cannot be captured. Pad chords take any button as the held one and
can do the quick menu, the volume, Close game, Save state, Load state, Reset game, Devices, Downloads
and Home (`docs/issues/guide-chord-configuration.md`); Settings › Guide chords lists them. One rule
(`gamepad/chord_gate`) serves the shell and the game: a chord's buttons never reach the game, and a
modifier tapped under 500 ms still reaches it as that button. Analogue triggers are L2/R2. Verified
by unit tests (uinput `pads_test` included) and a hidden run that bound Guide + B to Devices on the
page and opened Devices with it.

The Install folders rows show the folder each store will use and where it comes from: the store's
override, the default folder, or the store's own (GOG `<data>/opensu/gog-games/<id>`, Epic
legendary's `[Legendary] install_dir` else `~/Games`, Steam its default library while Steam runs).
The rows and the install flows read the same `InstallFolders::resolve`.

Chord list (S034): a modifier held alone for the hold delay shows its chords (partner glyph and
action) centred on the screen until it is released, plays a chord or is let out; the gate reports
the long hold (`gamepad/chord_gate:ChordGate::tick`). While a game runs, its mapping's chords
(S033) are listed after the shell's. Over a game inside Gamescope the overlay is
passive: drawn without `STEAM_INPUT_FOCUS` and without resting the pads (`app/game_screen`).
Verified by unit tests, a hidden run (`guide 2500ms` over the control channel shows `chordHint`
after the delay, the release opens nothing) and the `chord-hint` parity scene. Unverified: the list
over a real game in Gamescope and on a real pad. Outside Gamescope it is not shown over a game
(mapping the window would take the focus).

Unverified: live Steam, real Epic and GOG installs, volume writes on a real device, pad chords on
hardware (analogue triggers, chords over a game, game chords from a mapping (S033), a replayed tap
in a polling game), the in-game X11
path outside Xvfb, media keys.

### S033 — Button mappings

`input/button_map` (what each pad button sends, game chords sending pad buttons or a shell
action), `input/mapping_scopes` (game, else its emulator, else global; a whole map per scope; the
Steam Input switch, global and per game), `settings/mapping_store` (`mappings.json`, atomic). At
launch the shell resolves the game's map and holds the pads with it (`app/pad_owner`): the virtual
pad is remapped, the shell's controls are not, and `opensu-retro` reads the remapped virtual pad.
Game chords follow the shell's chord rule and join the chord list. The editor opens from a game's
details page (Controls) and Settings › Controls (global, per emulator, the Steam Input toggle)
as a controller board with a chip per button, a strip of chords, and a tile picker beside the
focused chip; while Steam Input has a game its mapping is refused and the page says why. Verified
by unit tests through the shipping code (`button_map`, `mapping_store`, `mapping_editor`,
`mapping_page`, `pad_translator`, uinput `pads_test`: a remapped A reaches the virtual pad as B
and a motion node is never grabbed) and the `game-mapping`, `mapping-picker`,
`mapping-listening`, `chord-picker` and `global-mapping` parity scenes. Gaps: key outputs
(XTest), how the switch reaches Steam (not measured against a running client), a quick menu
Controls row, and every check on hardware: a remapped real pad in a game, a Steam game reading
hidraw, gyro through the motion node, SDL HIDAPI doubled input
(`docs/issues/per-game-button-mappings.md`).

### S035 — Downloads page

One page (Guide › Downloads, a click on a launcher badge's progress ring, or the Downloads chord
action, unbound by default) lists Steam's queue, the store install openSU runs and the emulator or
core download, with state, progress, figures and the buttons each owner allows: Pause, Resume,
Download now, Move up, Move down and Remove for Steam through `SteamClient.Downloads` (the calls
Steam's own downloads page makes), Cancel for Epic, GOG, Steam's install wizard and emulator
downloads. Ended downloads stay listed until openSU quits; Epic and GOG installs stay one at a
time. `/state` lists the rows. Verified by unit tests through the shipping owners (stub
`legendary`, fake release host, fake Steam DevTools), a hidden run and the `downloads` parity
scene. Unverified: Steam's buttons against a real Steam client, a real Epic or GOG cancel, the page
over a game in Gamescope. Not done: cover art on rows, Epic and GOG bytes and time left. Details
in `docs/issues/downloads-page.md`. An uninstall (S037) is listed as Uninstalling, then
Uninstalled, with no buttons.

### S036 — Game updates

Checked when the catalog loads; openSU never updates a game by itself and a launch does not ask
first. An installed store game with an update has `Game::updateAvailable`: an arrow badge on its
tile, "Installed, update available" and an Update button after Play on its details page, and
Update after Launch in its tile menu. Update starts at once (`app::UpdateRequest`) through
`Installs::update`, which refuses as an uninstall does (ROM, not installed, running, another job)
and also with no update known, Steam not running, or a GOG game without a record.

- Epic: the installed `version` (`list-installed --json`) against `asset_infos.<platform>
  .build_version` (`list --json`), legendary's own `--check-updates` rule, from the two listings
  `library::epic` already runs. The update runs `legendary update <app> --update-only -y
  --skip-sdl` in `EpicInstallJob`.
- GOG: `gog-installs.json` records the build gogdl left (`gog::installedBuild`: `goggame-<id>.id`,
  else `.info` `buildId` for Windows; `gameinfo`'s version line for Linux, as `gogdl import`
  reads them); a record without one is read from those files at each listing. `library::gog`
  asks one request per installed game: the content system's builds for Windows (the build on no
  branch), `api.gog.com/products/<id>?expand=downloads`'s Linux installer version for Linux, as
  gogdl 1.3.1 downloads each (`gog_builds`). A failed request is logged and shows no update. The
  update runs `gogdl update <id> --platform <recorded> --path <recorded folder>` in
  `GogInstallJob` and records the new build.
- Steam: a manifest's StateFlags with StateUpdateRequired (2) on an installed app (this machine's
  `appmanifest_1628350.acf` reads 6 with `TargetBuildID` past `buildid`). Update asks
  `SteamClient.Downloads.ResumeAppUpdate(<app>, "0")`, as Steam's library Update button does
  (`steam::DownloadControl::moveToTop`,
  `app::SteamUpdateJob`); Steam's queue row on the Downloads page then carries it.

The Downloads page lists an Epic or GOG update as "Updating" with Cancel, like an install.
Verified by unit tests through the shipping owners (legendary 0.20.35-shaped JSON fixtures, a fake
GOG server, fixture manifests, stub `legendary` and `gogdl`, fake Steam DevTools) and the
`update-badge` and `details-update` scenes of the store run against a stand-in `legendary`.
Unverified: a real Epic, GOG or Steam update; whether Steam starts a queued update or leaves it
waiting (Steam's own library starts one with `ResumeAppUpdate`); whether GOG's Linux installer
version and `gameinfo` agree for every game. Details in `docs/issues/game-updates.md`.

### S037 — Uninstall

An installed store game's details page and tile menu offer Uninstall; the first press arms it
with what goes ("legendary deletes the Epic copy", "deletes the GOG copy at <folder>", "forgets
the GOG copy; <folder> is kept", "Steam uninstalls the Steam copy") and moving focus lets go; the
second starts it. Steam's goes through `SteamClient.Installs.OpenUninstallWizard([app], true)`
and waits for the manifest to go; Epic runs `legendary uninstall <app> -y`; GOG deletes the
recorded folder only when it lies inside `<install folder>/<id>` once symlinks are resolved, then
forgets the record. No Wine prefix, shared or own, and no ROM is deleted; a ROM has no Uninstall. It is refused
while the game runs, while an install or another uninstall runs, while Steam is not running and
for a GOG game without a record. On success the catalog reloads, so the game leaves Home. The
Downloads page lists it; `/state` has `armed` and the row's `uninstalling` state. Verified by unit
tests through the shipping owners (stub `legendary`, fake Steam DevTools, scratch GOG folders with
symlinks) and the `details-uninstall` and `downloads-uninstalling` parity scenes against a
stand-in `legendary`. Unverified: a real Steam uninstall (whether Steam raises its own confirm
dialog for `OpenUninstallWizard(..., true)`), a real Epic or GOG uninstall. Details in
`docs/issues/uninstall-games.md`.

### S038 — Windows games under Wine

A GOG or Epic install of a Windows build launches with `--wine <runner> --wine-prefix <prefix>`
(`library::wine::prepare`, from `ShellApp::launch` through `WineSettings`): gogdl and legendary
both take those flags. Every such game shares `<data dir>/wine-prefix`; a game's details page has
"Wine prefix: Shared/Own", and Own gives it `<data dir>/wine-prefixes/<store>-<id>`
(`settings.json` `ownWinePrefix`). The runner is Settings › Wine › Wine runner: Automatic (the
first found) or one of system `wine` and the Proton builds in Steam's `compatibilitytools.d` and
`steamapps/common`, used as their bundled `wine` binary; Steam's folders are only read. No runner,
or a picked one that is gone, refuses the launch with a toast. Steam games are left to Steam's own
compatibility tool. Verified by unit tests through the shipping owners against a fake PATH, fake
Steam folders and stub launches. Unverified: a real Windows GOG or Epic game launched this way;
a Proton build's `wine` used without its `proton` script gets no DXVK/VKD3D setup in the prefix.
Missing: "Use <compatibility tool>" for Steam games (writing `CompatToolMapping`), not built until
the running client's handling is measured (the card lists what).

### S026 — Emulator downloads

`assets/emulators.json` is the one catalog (`library/emulator_catalog`): detection and downloads
read it. Emulators with an official AppImage on their own release page carry a `release`: Cemu,
Eden (Forgejo, `stable.eden-emu.dev`), PCSX2, RPCS3, DuckStation, PPSSPP, Azahar, mGBA, xemu.
Dolphin, Ryujinx, shadPS4, melonDS and Xenia Canary are detected only: no official AppImage, or
one only inside an archive. No BIOS, keys or firmware are linked.

A download (`app/emulator_downloads`, `library/emulator_release`) goes to
`<data>/opensu/emulators/<id>/<asset id>/` through `net::WebClient`, is checked against the GitHub
asset digest and refused on a mismatch; without a digest (mGBA, Eden) only HTTPS from the
project's own host is accepted and the size and SHA-256 seen are recorded. The version in use
changes when `installed.json` is rewritten after the new file is verified; old versions go after
that. The update check shows versions and installs nothing. An AppImage whose runtime links
libfuse2 is refused with the distribution's install command when libfuse2 is missing; every
AppImage needs `/dev/fuse` and `fusermount`. `--appimage-extract-and-run` is not used.

Verified: tests through the shipping code against a fake release host (digest match and mismatch,
update kept apart until verified, cancel, remove, catalog parsing, merge with detected installs);
mGBA 0.10.5 downloaded from GitHub in a hidden openSU with scratch XDG dirs, from the ROM's
details page, then removed from Settings › Emulators. Unverified: a real update, a real Eden
download, launching a downloaded AppImage, versions of packaged emulators (not shown).

### S031 — Built-in emulator cores

A catalog entry with `"core"` is a built-in emulator (`library/emulator_catalog`): mGBA (GB, GBC,
GBA), Gambatte (GB, GBC), Snes9x, Mesen, PCSX ReARMed, and for Arcade FinalBurn Neo, MAME 2003-Plus,
MAME 2010 and MAME (current), listed after the standalone emulators so a system's automatic
emulator does not change where one is installed. Genesis Plus GX and PicoDrive are not listed:
openSU has no Mega Drive, Master System, Game Gear, Sega CD or 32X system
(`docs/issues/more-libretro-cores.md`). A core is found in
`<data>/opensu/cores/` (openSU's, with a `<core>.json` record), then `~/.config/retroarch/cores`,
`/usr/lib64/libretro`, `/usr/lib/libretro`, `/usr/lib/x86_64-linux-gnu/libretro`
(`library/libretro_cores`); the emulator is installed when its core is found and runs as
`opensu-retro --core <core> [--hidden] --saves <data>/opensu/saves/<system> <rom>` (`--hidden` when the shell is), which cannot start, with the reason, when `opensu-retro` is not
beside `opensu`. Downloads use the Settings › Emulators and details page paths of S026: the
buildbot's `.index-extended` is read, the zip is fetched with every hop on the buildbot's origin,
its one `.so` is checked against the listed CRC-32, which is the library's and not the zip's
(corruption, not a compromised host), written whole, then the record; the update check compares the listed CRC-32 with the record's.

Verified: tests through the shipping code against a fake buildbot (listing parse, lookup order,
CRC-32 match and mismatch, off-host redirect and off-host listing refused with nothing kept, an
on-host redirect followed, cancel, update detection and update, removal, LaunchSpec shape, the
ROM's emulator options, the details page Download then Play, Settings source, update and two-press
Remove). Every launch carries `OPENSU_RETRO_CONTROL` (S032). A hidden shell launched Kirby & the
Amazing Mirror from `/mnt/Boy/ROM/GBA/` on the mGBA core through `opensu-retro --hidden` (its
`/proc/<pid>/cmdline`, no `/dev/input` or sound fds) and closed it from the quick menu
(`scratch/retro-hidden/`). Unverified: a real buildbot download.

Arcade romsets: each arcade core names its ROM-set listing in the catalog (`"romsets"`: FinalBurn
Neo's arcade and Neo Geo datafiles and MAME 2003-Plus's and MAME 2010's `-listxml` from their
libretro repositories, current MAME's from the `mame*lx.zip` of mamedev's latest release, SHA-256
checked). They are fetched over HTTPS, kept under `<cache>/romsets/<id>.tsv` and fetched again
after 30 days (`library/romset_dats`). Each arcade zip's central directory is matched against
every listing, the parent's, BIOS's and devices' zips in the same folder counting, the default
BIOS choice only, disks as `<set>/<disk>.chd`; verdicts are kept in `<cache>/romsets/matches.json`
by the size and modification time of every file looked at (`library/romset_match`). The game runs
on the search's pick when it matches, else the first matching core installed, else the first
matching core is offered for download; with none the game is listed, cannot start ("No arcade
core matches this romset") and the details page names the nearest core and the files it lacks. A
system's emulator pick gives way to a matching core; a game's own pick does not. The details page's
rows go beside the buttons when they do not fit above them.

Verified (arcade, 2026-10-20, `scratch/arcade/`): unit tests (`romset_match_test`: datafile and
`-listxml` parsing, the line format round trip, parent, renamed member, BIOS choice, device of a
device, disk, kept verdicts invalidated by a parent zip appearing and by a new listing, the pick
and the no-match text; `details_page_test`, `emulator_catalog_test`, `zip_archive_test`). A hidden
shell with `OPENSU_ROM_ROOTS=/mnt/Boy/ROM` fetched the four listings (8,456, 5,409, 8,833 and
50,368 sets) and logged `romset armwar: MAME 2010 (built-in) matches` and `romset avspu: MAME 2010
(built-in) matches`, the others lacking `armwar.key` (FinalBurn Neo, MAME 2003-Plus) and
`armwar.key, dl-1425.bin` (MAME), and for avspu the 1 MB `avp.14m`-`avp.20m` of the later sets. The
details page Download fetched MAME 2010 into the scratch data dir; avspu and armwar ran on it
(`/status` frame counts rising about 59.6 a second) and closed from the quick menu
(`final-avspu-details.png`, `final-armwar-details.png`, `final-armwar-status.txt`,
`avspu-runner-frame.png`, `armwar-runner-frame.png`). A copy of avspu named `avsp.zip` showed "No
arcade core matches this romset" with "MAME 2010 (built-in) lacks 2 of 15 files: avpe.03d,
avpe.04d" and refused Play with that toast (`final-avsp-play-refused.png`). Overridden per game,
FinalBurn Neo, MAME 2003-Plus and MAME (a 416 MB core) downloaded and loaded armwar in the runner:
FinalBurn Neo ran it without its key, MAME 2003-Plus refused `armwar.key` and MAME
`armwar.key`, `dl-1425.bin`, the files the match named. Unverified: a romset that matches
FinalBurn Neo, MAME 2003-Plus or current MAME running to gameplay (none in the player's folder).

The listings download off the library scan (`library/romset_lists`): arcade games are listed at
once, a game with no match yet while a listing downloads shows "Checking romset…" and its Play says
the lists are downloading, each listing is a Downloads page row ("Romset list · <core>", bytes and
percent, then Finished with its size or Failed with the reason and when it is tried again), each
end is toasted and the catalog is read again. Verified (2026-10-20, `scratch/romset-downloads/`,
empty cache, `OPENSU_ROM_ROOTS=/mnt/Boy/ROM`, hidden shell): the first catalog (87 games) logged all
eight armwar/avspu verdicts `pending` while FinalBurn Neo's list downloaded; the Library showed the
Arcade folder while MAME 2010's list was at 37 % (`02-…`); armwar's details read "Checking romset…"
and Play toasted the reason (`10-…`, `11-…`); the page showed MAME 2003-Plus at 55 % then 77 % of
22.6 MB with the rest queued (`12-…`, `13-…`) and all four finished (17.1, 22.6, 43.4, 19.7 MB,
`09-…`, `14-…`); armwar then read "Matches MAME 2010 (built-in)" (`04-…`, `15-…`), played on MAME
2010 (`armwar-runner-status.txt`, runner frames 1470, 1589, 1709 two seconds apart) and closed from
the quick menu ("armwar closed"). Tests: `romset_match_test` (`RomsetLists` against a loopback
host, pending verdicts), `downloads_board_test`, `downloads_controller_test`, `web_client_test`.
Gap: the toast's text draws very small (also in the earlier `scratch/arcade/` frames).

### S032 — The libretro runner

`opensu-retro` (`src/retro/`, design in `docs/design/libretro.md`) loads one core, answers its
environment (directories, pixel formats, core option defaults v0 to v2, log, geometry and timing),
runs it at its rate on a clock, draws its frame with Vulkan letterboxed in an SDL window, plays its
sound at 48 kHz through a resampling ring with ±0.5 % rate control, reads every pad without grabbing
(the shell's virtual pad included) and the keyboard, and keeps save RAM in `<saves>/<rom>.srm`,
written atomically when it changes and at exit. Phase 2: with `OPENSU_RETRO_CONTROL` set (the shell
sets it for every launch) it serves `/status`, `/state/save`, `/state/load` and `/reset` on a free
loopback port announced in that file, runs each command between frames, keeps one state slot
`<saves>/<rom>.state`, and leaves a load failure in the file (`retro/control_server`,
`retro/save_state`). The quick menu shows Save state, Load state and Reset game for such a game
(`app/retro_session`, `app/quick_menu_controller`); a game in an Instance that dies by a signal
other than TERM, KILL, INT or HUP is toasted as crashed, by name, and a runner's load failure by its
reason (`launch/handoff` `Hooks::ended`, `app/game_end`). A game's button mapping (S033) reaches
the runner through the shell's virtual pad, so `retro/joypad` keeps one fixed table.

Verified: unit tests through the shipping code (pixel formats, rate control against a device 0.3 %
fast and slow, the joypad mapping, save RAM round trip, failed and wrong-size writes, every answered
and a refused environment command, a fake core through `Core`), and `runner_test` running the real
binary hidden with Vulkan validation on: the fake core's RGB565 bars letterboxed in the frame PNG,
its `.srm` continued across two runs, exit 2 for a missing core or ROM, a crashing core killing only
the runner by SIGSEGV; the control file with pid and port, `/status`, 404 for a load before any save,
a save writing the `.state`, a later load undoing the frames between (the `.srm`'s count), reset,
409 with the reason from a core that cannot serialize, SIGTERM removing the file, a missing ROM
leaving `{"error"}`. `retro_session_test` and `quick_menu_controller_test` against a fake runner
(rows, disabled Load state, two presses, refusals), `handoff_test` (a scope process killing itself
with SIGSEGV reported as 139, a clean exit as 0, a force-close not at all), `game_end_test`
(wording). By hand, hidden: the buildbot's mGBA core with Kirby & the Amazing Mirror answered
`/status`, 404 before a save, wrote a 430 KB `.state`, loaded it and reset, and removed its control
file at exit. By hand: the buildbot's mGBA core (its listed CRC-32, which is the extracted
`.so`'s, matched) ran Kirby & the Amazing Mirror hidden for 600 frames with no validation errors; the
frame PNG shows the title screen letterboxed 3:2 and the 32 KiB `.srm` was written. The fake core's run
also passes on lavapipe with validation.

Unverified: a visible window under Gamescope with sound on a device, a real pad and the shell's
virtual pad driving a game (`pads_test`'s new reader case cannot pass on this machine while the
running openSU holds every new pad; the pre-change test binary fails the same way). Missing: ports
beyond the first, analogue sticks (`RETRO_DEVICE_ANALOG`), core option changes. Unverified: the quick menu's
state rows over a real game in Gamescope, and the crash toast after a real crash from the shell.
Control requests run on the shell's loop, so a runner that stops answering holds the shell for up to
5 s (the loopback client's socket timeout).

### S027 — Steam libraries

Settings › Steam libraries (`app/steam_library_settings`, `steam/libraries`) lists Steam's
libraries through `SteamClient.InstallFolder.GetInstallFolders`: path, free space, games, size,
the default. Add uses `AddInstallFolder`, make default `SetDefaultInstallFolder(index)`, remove
`RemoveInstallFolder(index)`, refused for Steam's own library and for one that holds games, as
Steam's own UI does. Steam also exposes `MoveInstallFolderForApp`, `SetFolderLabel`,
`RepairInstallFolder` and `RefreshFolders`; moving games is not offered. Without a running Steam
the page lists `libraryfolders.vdf` read-only and says Steam is not running. Verified against a
fake DevTools Steam in tests; not against a live Steam.

### S028 — Lua plugins and Eden play time

`src/plugin/` runs sandboxed Lua 5.4 plugins (`docs/design/lua-plugins.md`): one state per plugin
on its own worker, I/O on a shared pool, memory, instruction and run-time limits, three faults in a
row stop a plugin. API v1 has play-time sources and emulator game lists. Settings › Plugins turns
plugins on and off; a user plugin asks for consent in the session dialog first. The bundled
`eden-playtime` reads `playtime.bin` and Eden's game-list cache.

Verified: tests through the shipping host (sandbox escapes, limits, faults, read-path and HTTP
refusals, the shipped Eden plugin against fixture files, the merge, the consent flow); a `--hidden`
run with scratch `XDG_CONFIG_HOME` and `XDG_DATA_HOME` and Eden's real data reachable read-only
logged "play time for 11 of 27 Switch games", and Super Mario Odyssey's details page showed
"Play time 4 h 45 min" (17145 s). Settings › Plugins rendered with the plugin on, its state and
its read paths.

Gaps: Eden's game-list cache stores program IDs with no file paths, and titles only for NAND
installs, so a Switch dump without a `[0100…]` tag in its name gets no play time (5 of the 16
games Eden has time for on this machine: Mario Kart 8 Deluxe among them). The fix is reading the ID
from the dump (NSP ticket names in the PFS0 header). Unverified: Flatpak Eden paths, HTTP against a
real server (tests use a loopback host), a user plugin's consent flow on screen, and a C-side
pattern stall (it would wedge only that plugin's worker). Settings › Plugins notes are truncated at
the row width.

### S029 — CI

`.github/workflows/ci.yml` runs `tools.check ci`: the `packaging/ci` image (Fedora 44, the release
the fork's local podman build uses) boots systemd, the user manager starts for the runner's uid,
and `tools.check all --gamescope native` runs as that user: Lucent and gpu2d checked out by
commit, configure with Clang (any other compiler is refused), build including the Gamescope fork,
clang-format, clang-tidy, ruff, ctest. A second job builds and smoke-tests the AppImage (S030).
Actions are pinned by SHA, uv by version and checksum, permissions are `contents: read`.
No Windows or macOS jobs: openSU is Linux only (see the comparison baseline).

Verified: the same steps locally through `tools.check ci`: Clang build including the fork, the
three linters clean, 117 of 119 tests pass and 2 skip (`game_keys`, `pads`: no uinput in the
container). Unverified: a run on GitHub's runners
(rootless podman with systemd as PID 1 there).

### S030 — AppImage

`tools.appimage` builds in an Ubuntu 24.04 container, so glibc 2.39 is the floor (a check refuses
any bundled ELF needing newer), with Clang 22 and wayland, wayland-protocols, libxkbcommon, pixman
and libdrm headers from pinned tarballs for the fork's wlroots. Contents: openSU, its assets, the
sign-in `.xpi`, the fork's `gamescope` in `usr/libexec/opensu/`, the desktop entry and icon, and
the libraries not on every desktop (62, `bundle.HOST_PROVIDED` lists what the host supplies: glibc,
libstdc++, GPU drivers, GL, EGL, Vulkan, libdrm, gbm, X11/xcb, udev, systemd, curl, PipeWire,
ALSA, PulseAudio, GLib, D-Bus). Runtime: the static type 2 one, no libfuse2. `AppRun` is a
symlink to `usr/bin/opensu`, so `/proc/self/exe`-relative assets and Gamescope resolve inside
the mount. A `v*` tag runs `release.yml`: build, smoke, GitHub Release with the AppImage, its
`.sha256`, `.zsync` and the `.xpi`; the AppImage is gpg-signed with the
`OPENSU_APPIMAGE_SIGNING_KEY` secret (public key `packaging/appimage/signing-key.asc`).

Verified (`tools.appimage smoke`, clean Fedora 44 and Ubuntu 24.04 containers with only the
README's runtime libraries, no network): the runtime is static type 2 and libfuse2 is absent; it
mounts through FUSE; every bundled ELF resolves; the fork's `gamescope --help` binds every symbol;
`opensu --render` draws the home screen at 1280x800 with assets; the run writes only under the
XDG directories. A signature from a throwaway key is embedded and checks out.

Gaps: session mode is not installable from the AppImage (the `session` install component is left
out; its entry would name a mount point that changes per run). Fedora's libcurl has no symbol
versions, so the dynamic loader prints "no version information available" for libcurl on every
start there. A Gamescope session itself (nested or own) from the AppImage is unverified, as is a
release run on GitHub.

### S039 — Opt-in integrations and Steam modes

`settings::Integrations::inForce` turns the saved `integrations` choices into what is in force: no
choices is everything off and opens the first-run chooser (`app::FirstRunChooser`); Steam's mode is
Normal or Managed on a desktop, Managed alone with `--session`. `app::IntegrationSwitch` applies a
change live: the catalog's providers are replaced, the plugins host is made or dropped, standalone
emulator discovery and downloads follow, and Steam is stopped and started in its new mode. Normal
attaches to the desktop Steam (`~/.steam/steam.pid`, a live `steam` process) or starts one on the
desktop outside openSU's scope; the shell then neither holds the pads against it nor closes Big
Picture. Verified by unit tests (`integrations`, `integration_flow`, `integration_chooser`,
`settings_controller`, `client`, `handoff`, `catalog_loader`, `focus_owner`) and a hidden run driven
over the control channel with a scratch home (`scratch/integrations/drive.py`): the chooser on a
fresh settings file, only ROM shelves and no Steam process with all off, Steam tiles from a fake
root in Normal and Managed with the mode in the log, Epic tiles and `legendary` calls only while
Epic is on, and a fake desktop Steam pid file found and logged (a hidden run attaches to nothing and starts nothing). Gap:
Isolated mode is not built (needs the sandbox measurements in the card); Normal mode has not run
against a real desktop Steam, and its installs need the desktop Steam started with
`-cef-enable-debugging`.
