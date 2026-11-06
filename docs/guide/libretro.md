# The built-in libretro runner

`opensu-retro` runs one game in a libretro core, in its own window drawn with Vulkan (MoltenVK on
macOS): the built-in emulator openSU launches like any other. It reads every pad without grabbing
it, the keyboard and, on Android, its own touch controls; plays the core's sound at 48 kHz; and
keeps save RAM in `<data>/opensu/saves/profiles/<profile>/<system>/<rom>.srm`, one folder per
player profile, written when it changes and at exit. Saves from before profiles move into the
first profile's folder once, backed up first under `~/.local/state/opensu/save-backups/before-profiles/`.
It and the ROM library are the two parts of openSU that are always on.

![uCity with the crt-royale preset](../screenshots/retro-crt.webp)

## Cores

The built-in emulators are mGBA (Game Boy, Game Boy Color, Game Boy Advance), Gambatte (Game Boy,
Game Boy Color), Snes9x, Mesen, PCSX ReARMed, Beetle PSX HW and SwanStation (PlayStation),
Mupen64Plus-Next and ParaLLEl N64 (Nintendo 64), PPSSPP (PSP), Flycast (Dreamcast), melonDS DS
(Nintendo DS), and for Arcade FinalBurn Neo, FB Alpha 2012, MAME 2003-Plus, MAME 2010 and MAME
(current). A
console's standalone emulators come first, so turning on standalone emulators does not change what
an installed one runs on. GPU cores (Beetle PSX HW, SwanStation, Mupen64Plus-Next, ParaLLEl N64)
draw on the runner's Vulkan device; a core that asks for OpenGL is refused by name.

A core is found in `<data>/opensu/cores/` (openSU's own downloads), then on Linux in
`~/.config/retroarch/cores`, `/usr/lib64/libretro`, `/usr/lib/libretro` and
`/usr/lib/x86_64-linux-gnu/libretro`. A missing core is downloaded from libretro's buildbot from the
game's details page ("Download <core>") or Settings › Emulators › Emulators and cores: every hop
stays on the buildbot's host and the library is checked against the CRC-32 the buildbot lists.
Check for updates compares the listed CRC-32 with the one recorded; nothing updates by itself.

## Arcade romsets

Each arcade core names its romset listing: FinalBurn Neo's arcade and Neo Geo datafiles, FB Alpha
2012's 0.2.97.29 datafile, MAME 2003-Plus's and MAME 2010's `-listxml`, and current MAME's from
mamedev's latest release
(SHA-256 checked). They are downloaded in the background, shown on the Downloads page, kept under
`<cache>/opensu/romsets/` and fetched again after 30 days. Each arcade zip is matched against every
listing, with its parent's, BIOS's and devices' zips in the same folder counted. A FinalBurn listing
names every BIOS a game could use, so one of them is enough there. The game runs on
the first matching core installed, else the first matching core is offered for download; with none
it is listed but cannot start, and its details page names the nearest core and the files it lacks.
This works in a folder picked on Android too, where a game's parent and BIOS zips are copied beside
it when it starts. FB Alpha 2012 keeps no `.srm`: a game's settings and high scores are in
`<rom>.nv` in the same folder, and it plays older sets without CPS2 `.key` files.

## BIOS

Settings › Emulators › BIOS files lists what each built-in core runs on: the files it looks for,
which of them you have in `<data>/opensu/system/`, and whether it has a built-in HLE BIOS
(PCSX ReARMed's PlayStation BIOS is "HLE built in" until you add a file). openSU ships and downloads
no BIOS, keys or firmware.

## Picture: video settings and shader presets

Every built-in core's game has, under Video:

- **Aspect**: Core, 4:3, Pixel 1:1 or Stretch.
- **Integer scale**: Off, On (whole multiples of the game's pixels) or Overscale (fills and crops).
- **Sampling**: Nearest, Bilinear or Sharp bilinear.
- **Filter**: None, Scanlines, LCD grid or CRT (a port of crt-lottes), drawn on the game's own pixel
  grid.
- **Shader preset**: a RetroArch slang preset, which replaces Sampling and Filter.
- **Shader resolution** (shown with a preset): Auto (the default) draws the preset at a lower
  resolution, down to half, only while the GPU cannot finish a frame in time, and at full
  resolution again once it can; a PC that keeps up never changes. Full always draws it at the
  screen's resolution, however slow.

The presets come from libretro's slang shader pack (about 54 MB, each shader under its own
licence): Settings › Emulators › Shader presets downloads it into `<data>/opensu/shaders/slang/`,
checks for a newer build and removes it. The preset picker browses its folders (`crt › crt-royale`,
`pixel-art-scaling › sharp-bilinear-simple`, `bezel › Mega_Bezel` and the rest). The runner builds
presets with librashader, shipped beside it; without it a game with a preset refuses to start and
says why.

## Setting layers

Every per-game setting (video, the core's own options, the emulator pick, frame generation and
resolution) is kept at four layers: Global, the console, the core, and the game. A value resolves
game, then core, then console, then Global, then the default, and every row names the layer it
comes from. One editor shows them all, with a tab per layer: Settings › Emulators (Global, Consoles,
Built-in cores), a game's details page (Game settings) and the Guide menu over a running game (Game
settings, a panel over the game, whose changes reach the game at once).

![The layered editor over a game](../screenshots/layered-editor.webp)

In the editor, choosing a value sets it at the tab's layer; every list starts with "Use <value>
from <layer>", which removes the tab's own value (Y does the same, and Y again puts it back); X goes
to the tab the value comes from; LB and RB change tabs. Each change is saved at once. The settings
live in `game-settings.json` in openSU's config folder.

A core's own options (Color Correction on mGBA, the renderer on Mupen64Plus-Next, ...) are settings
of the core's and the game's layers, remembered per core so they can be edited while nothing runs.

## Over a running game

The Guide menu over a built-in core's game adds Save and close, Save state, Load state (one slot,
`<saves>/<rom>.state`) and Reset game, the last two asking first, and Game settings opens the
editor above. Close game stops the runner so it writes its save RAM first. A core that crashes is
toasted by name, and a game that fails to load by its reason.

Save and close keeps the game where it is in a resume state of its own
(`<rom>.resume` beside the profile's `.srm`, never your Save state slot; each profile has
its own), writes its save RAM and closes it. It travels with the game's save to paired openSUs and
shared folders, so Play on another machine picks the game up where you left it; a game that keeps
no save of its own resumes only on the machine it was closed on. Starting
the game again puts you back there; the Guide menu then shows "Resumed session" with
the date. The resume
state is used once: it is removed as it loads, on every machine once they next sync, so a game
that crashes later starts from its own save next time. While one is waiting, the game's details page reads "Resumes from <date>" and
offers Start fresh, which throws it away and starts the game from the beginning. A resume state is
not loaded, and the Guide menu says why, when another core wrote it (it is kept for that core),
when the core refuses it, or when the game's save changed since (a copied or synced save is never
undone by it). Close game keeps no resume state.

![The Guide menu over a game](../screenshots/guide-game.webp)

## Two players

On a GBA game's details page, Play linked (2 players) runs two linked GBAs side by side on the mGBA
link core, each pad its own player. Player 2's save is `<rom>.p2.srm` beside player 1's. Play with
<name> does the same with a paired openSU ([LAN](lan.md)), a cartridge's clock following the emulated
game there rather than your computer's. Game Boy games do not link yet, and single play stays on the
emulator you picked.

## On Android

The runner is a second Activity in a process of its own. Cores come from the buildbot's
`android/latest/<abi>` folder, games are copied into the app's cache before a core opens them, and
the runner draws touch controls while no pad is connected. An arcade game shows its own machine's
panel: an arcade stick, round buttons in the cabinet's number and rows, each named for what it does
in that game when the core says, and small Coin and Start controls, since nothing starts without a
coin. It sits on the black beside the picture where it fits, and hides like the rest when a pad
connects. Every other system shows the usual pad. Back, a pad's Guide or the pause button
brings the shell's Guide menu in front of the paused game; Resume goes back to it.
