# Project state

## Comparison baseline

The baseline is what this machine does today with no openSU installed: Steam's
own Big Picture (`steam -gamepadui`) launched under Gamescope, switched to by
hand from a Fedora KDE Plasma session. That UI lists Steam only. Epic needs
Legendary or Heroic in a separate window, GOG needs Heroic (or gogdl and a script), and emulators and
ROMs have no home at all.

Visible deltas from the baseline:

- Epic games (Legendary) and the player's GOG library (openSU's own sign-in) sit in the
  same grid as Steam; emulator ROMs sit behind one tile per console in the Library section, and
  each store and the combined "All games" library behind a tile of its own there.
- A dock (Home, Library) switches sections with L1/R1; Library has Standard, XMB and Carousel
  layouts, chosen with START and kept in the config dir.
- Each launcher's state is a logo with a status dot inside the top-right status pill, where iiSU
  has its notification bell (openSU has no notifications, so the bell and its R2 glyph are gone).
  A badge is a pointer target: hover lights it, a click selects that store's tile in Library.
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
  Hide/Unhide is in the context menu (Select on a pad, Tab on a keyboard, right click on a tile),
  with Launch or Install and Details; hidden games leave Home, Library, folders, All games and
  search except under the hidden filter. The last-played time of an Epic, GOG or ROM launch is
  recorded by openSU. Library's icon size is iiSU's slider (levels 1 to 20, default 9) in the
  options for all three layouts. START also opens the options on Home now, and Y still shows the
  details.
- The corner prompts name only what works: Back inside a folder, Details with a game focused,
  Select on a tile, Menu on Library. iiSU shows them all always.
- The title pill names the focused tile on Home too; iiSU shows it only inside sections.
- A signed-out GOG or Epic tile opens that store's sign-in page in the default browser.
- No store client window is ever opened to reach a game.
- The grid is ours, so layout, tile sizes and page count are ours.

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
| S010 | Own login session entry on Gamescope | missing | — | G004 |
| S013 | openSU runs in a nested Gamescope inside KDE at the output's resolution | partial | S004 | G004 |
| S014 | Alt+F4 closes the game in nested mode while Alt+Tab stays with KDE | partial | S013 | G004 |
| S015 | Shell owns every instance it starts and can force-close it from the pad | partial | S004 | G003 |
| S017 | Per-game render resolution with Gamescope FSR upscaling, Deck-style | missing | — | G004 |
| S016 | Steam client started and owned by openSU, with its state in the top bar | partial | S015 | G003, G004 |
| S011 | Steam's own components kept out of the game grid | partial | S002 | G001 |
| S019 | iiSU's UI sounds on the events openSU shares with iiSU | partial | S005 | G002 |

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
played through raylib's audio device (`audio::SoundPlayer`). Always on; there is no volume or
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
`navigation.md` 1.2-1.5, 4) is a bottom-centre glass capsule with Home and Library icons (iiSU's
own nav drawables, taken from the pinned APK into `<cache>/artwork/nav/`) and LB/RB badges, in the
variant matching the chrome (light). It is shown always on Home, and on Library while pinned (default) or else for 1200 ms after
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
recoloured with the focused console's border colours and a marker, and inside a console a 72 dp card
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
three the title pill names the focused game.
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
no separate load step.

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
title id, two Steam apps without either image). Switch and arcade have no source;
their ROM tiles keep iiSU's first-letter fallback.

A ROM's tile title is `library::roms::cleanTitle` of its file or folder name: everything from
the first `(` or `[` goes (region, language, revision, dump flags, `Decrypted`, `-patched`), a
release number prefix goes, a trailing `, The` (also A, An, Le, La, Les, Il, El, Der, Die, Das,
Los, `L'`) moves to the front, and a name without spaces reads its underscores as spaces. The
artwork key stays the untouched name, so box art matching is unchanged. An arcade system's zip
is a MAME short name, so its title is the description libretro-database lists for it
(`sf2` is "Street Fighter II: The World Warrior (World 910522)"), cleaned the same way. The
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
Gaps: a ROM whose file name is a code on a system other than arcade (a Vita `PCSE00905` pkg,
a PS4 `CUSA` folder) keeps that name; the Vita table (`psvita_title_ids.json` in the APK) is
the candidate. A hash lookup is not done: the listings carry the CRC of the whole zip, which
differs between a user's re-zipped copy and the listing (`armwar.zip` and `avspu.zip` here match
neither), so it would only work for TorrentZipped sets. A game the two listings lack (newer than
MAME 2016 and not in FinalBurn Neo) keeps its short name. Arcade box art has no source yet:
libretro's MAME thumbnails are named by these descriptions, so the key could use them.

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
Wayland window, which this machine's screenshot tooling cannot see. raylib
renders to a texture and exports it, so a frame is a file.

Type is Cal Sans 1.000, iiSU's default face (`pp4.a`; the others it offers are Console Sans
and Arctainium), shipped in `assets/` under the OFL with the licence beside it. The file is the
Google Fonts build, which matches the one in iiSU's APK in metrics and Latin-1 outlines. Text
sizes are Android's: the em in pixels, with letter spacing in ems; `Typeface` converts to
stb_truetype's ascent-to-descent pixel height from the face's `hhea` table.

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
Steam's virtual pad ignored, disconnect), `direction_repeat`, `pad_translator`.

The raylib/GLFW reader this replaced had no mapping for the xone driver's Xbox
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
`railTileAt`, `ChooserLayout::cardAt/rowAt/iconSizeAt`, `SearchLayout::keyAt/resultAt`, `ContextLayout::itemAt`, `GameMenuLayout::itemAt`, `PanelLayout::hintAt`, `dockItemAt`.
Tested: `home_layout`, `rail_layout`, `mode_chooser`, `search_panel`, `context_menu`, `game_menu`, `launch_panel`, `pointer_router`,
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
Hiding is a request to the main loop, not a call from the handoff thread: raylib's
window calls belong to the thread holding the GL context, and there is no queue
that makes them safe from elsewhere. The launch thread only raises a flag. Every source records a `processHint` — the
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
  <path> <id> --platform <p>`, plus `--wine wine --wine-prefix <data dir>/gog-prefixes/<id>` for
  a Windows install. No uninstall, update or repair yet; a deleted game folder stays recorded.

Unverified: no real GOG download was made. gogdl's output format, the folder layout under
`--path` and `gogdl launch` come from reading gogdl 1.3.1's source and a fake gogdl in
`tests/app/install_job_test.cpp`; the `worksOn` field comes from minigalaxy's use of the same
endpoint and is only exercised against the fake GOG server in `tests/library/gog_test.cpp`, so a
real account's answer may differ (a product without it has no builds and cannot be installed).
Windows installs run under the system `wine`, not Proton.

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
`GET /state` returns the shell's state as JSON, `POST /input` queues a tap (press and release) of a button by
name, `GET /frame.png` returns the next frame as PNG bytes, `POST /quit` closes
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

Missing: the raylib reader that had it is gone; evdev force feedback is the way back.

### S010 — Own login session

There is no session entry, so openSU can only run as a window inside another
session.

### S013 — Nested Gamescope session

Outside Gamescope (`Config::insideGamescope`, from `GAMESCOPE_WAYLAND_DISPLAY`)
`session::NestedSession` runs openSU itself as `gamescope -W -H -w -h -r -f --
opensu ...` with the current monitor's size and refresh from raylib
(`session::readMonitor`), in the scope `<session>-compositor.scope` with
`OPENSU_SESSION` set for the inner opensu. When Gamescope ends it stops every
other scope named `<session>-*`, and SIGINT/SIGTERM stop the session through its
scopes. Games are never wrapped in a Gamescope of their own. The argument vector
and the session's run, leftover cleanup and signal path (against a fake
`gamescope`) are tested; no nested session has been run against a display yet.
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
to commit `41e84d4f5870a06534e310ff279a19a137375f79` (3.16.29 plus one commit).
It adds `--close-focused-window`: a host close request sends `WM_DELETE_WINDOW`
(or the xdg close) to Gamescope's focused app window and the session keeps
running. `gamescopeArgs` always passes it. `cmake/Gamescope.cmake` builds the pin
as an ExternalProject (meson, release, Clang; the layer and the tests are off) whose
configure and build steps run in a rootless podman container built from
`packaging/gamescope-build/Containerfile` (host's Fedora release, `dnf builddep gamescope`;
image tagged by the Containerfile hash), then checks the staged binary with `ldd` on the host
(Fedora hosts only; no host-build fallback) and stages it where `Config::gamescope` finds it, in the build
tree and installed.

With no game running, the focused app window is openSU's own, so Alt+F4 reaches
raylib as a window close: `WindowShouldClose` ends the frame loop in
`ShellApp::run`, which stops the control channel, calls `CloseWindow` and
returns, the inner openSU exits, Gamescope ends with its only client and
`NestedSession` stops the session's scopes. That is the intended desktop behaviour.

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

Guide while a game runs opens a menu down the left edge over the dimmed game:
Resume, Close game (`ui::GameMenu`, `GameMenuPainter`); Up/Down move, A selects,
B or Guide resumes, and other buttons do nothing while a game runs. Shift+Tab, Steam's
overlay key, is a Guide press (`session::GameKeys`): Gamescope gives a game's keys to
the game's window on the Xwayland openSU shares, and XInput2 raw key events on the
root reach openSU too, whatever has focus and only while the desktop gives Gamescope
the keyboard. Nothing is grabbed, so the game also sees the Shift+Tab. Tested:
`game_keys` (the chord, and raw keys from XTest against Xvfb); verified headless with
`xdotool` on Gamescope's Xwayland during a running game (Tab alone does nothing,
Shift+Tab opens the menu and closes it). Not yet pressed on a physical keyboard
through nested Gamescope. Inside Gamescope
openSU's window stays mapped as Gamescope's overlay (`session::GamescopeOverlay`:
`STEAM_OVERLAY`, `_NET_WM_WINDOW_OPACITY` 0 while the menu is closed, and
`STEAM_INPUT_FOCUS` while it is open); its window is sized to the output and has an
ARGB visual, which is why it has no MSAA. Outside Gamescope the window is hidden
during a game and shown for the menu. Verified headless: `gamescope --backend
headless` running openSU with a ROM whose emulator execs `glxgears`, driven over the
control channel (launch, guide, down, a), with full-composition screenshots
(`GAMESCOPECTRL_REQUEST_SCREENSHOT` = 3 on the root; `gamescopectl screenshot`
drops overlay planes). Driver: `scratch/overlay-test/drive.py`.

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
Emulators (`library/emulators`) are found on PATH, as AppImages in
`~/Applications`, `~/AppImages`, `~/.local/bin`, or as Flatpaks; a system without
one is listed, and play names what to install. On this machine: 124 games in the
grid from `/mnt/Boy/ROM`; Dolphin (GameCube), PCSX2 (PS2) and Eden (Switch)
launched in a headless Gamescope, showed a window in 1.5–2.5 s and closed from the
Guide menu (`scratch/roms-real/smoke.py`). Cemu, RPCS3, shadPS4 and Xenia Canary
arguments come from their `--help` and are not launched yet. Not covered: PS5,
Vita `.pkg` (needs installing into Vita3K), XBLA, Amiga disk sets, Android.

### S016 — Steam client

Steam gets a DBus session bus of its own: on the desktop's bus it registers a tray
item (`org/ayatana/NotificationItem/steam`) that KDE shows on its panel, although
its windows stay inside Gamescope. Measured with a real Steam in a headless
Gamescope: the item appears on the shared bus and not on a private one, logon still
completes, and `-applaunch`/`-shutdown` still reach it through its pipe.

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

`steam::Client` starts `dbus-run-session -- steam -silent -cef-enable-debugging` in `<session>-steam.scope`
when openSU starts, if a Steam install exists, and watches it: Initializing until a logon line
(`[Logged On` with `RecvMsgClientLogOnResponse() : processing complete`) is
appended to `$HOME/.steam/steam/logs/connection_log.txt` after the start, Failed when
the scope empties, Blocked when `$HOME/.steam/steam.pid` names a live client outside
opensu. Steam launches wait for Ready, and are refused with a named message when
Blocked or Failed. On exit it runs `steam -shutdown`, waits up to 20 s, then stops
the scope. The state shows as the Steam badge in the top bar's friends slot (S005)
and in `/state` as `steam`. Tested with a fake home and fake `steam`; a real Steam
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
