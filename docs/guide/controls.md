# Controls and shortcuts

Controllers are read from evdev on Linux: any pad whose driver follows the kernel's gamepad codes
(xpad, xone, hid-playstation, hid-nintendo, ...) works, keyboards are never taken for one, and a
pad switched on while openSU runs is picked up. Elsewhere pads come through SDL. The mouse works
everywhere, and on Android touch: a click or tap focuses and presses.

## In the shell

| Pad | Keyboard | Action |
| --- | --- | --- |
| D-pad or left stick | arrows | Move focus; Up or Down past a list's end goes round to its other end; A opens a choice's list |
| A | Enter, Space, click | Open a game's details page; Play, Install, change a ROM's emulator or Hide from there |
| B | Escape | Back; on Home with nothing to go back to, the Guide menu |
| Y | Y | Details page |
| X | F | Refresh the library |
| Select | Tab, Menu key, Shift+F10, right click | The focused tile's menu |
| Start | E | The Guide menu on Home and Library, for pads and tablets with no usable Guide button |
| LB / RB | `[` / `]` (R for next) | Previous / next section: Home, Library |
| | S, `/`, Ctrl+F | Search across every store, from Home and Library; with the keyboard and mouse, or a finger, the bottom-left corner shows it (S, or a magnifier to tap). A pad opens it from the Guide menu |
| Guide | Shift+Tab | The Guide menu: Home, Library, Search (off a game, over any page), Devices, Downloads, Settings, Power; places and actions only |
| Guide + A | Ctrl+Tab | Quick settings: a panel at the right edge, over a game too, with icon tabs of categories of settings you arrange (see below) |
| Guide + Up / Down / Left | Ctrl+Up / Ctrl+Down / Ctrl+M | Volume up / down / mute |
| Hold Guide (or any chord's held button) | | After half a second, a list of that button's chords, also over a game in session mode; it goes when you let go or play a chord |

Guide › Power › Quit openSU closes openSU, asking first; as the login session Power has Switch to
desktop instead. openSU also closes the way your system closes an app (Alt+F4, ⌘Q on a Mac, the
window's close button). Either way a game that is running closes first, as Close game closes it,
and openSU closes once it is gone; closing again while it goes closes openSU at once. Inside
openSU's Gamescope, Alt+F4 closes whichever window is in front, so over a game it closes the game
and openSU stays.

## Typing and pasting

Every text field (a search, a path, a key, a password, a share's address, a profile's name) takes
a physical keyboard, the on-screen keyboard and the clipboard. Paste with Ctrl+V (Command+V on a
Mac), Shift+Insert, a right click or a long press on the field, or from a pad with the regular
grid's Paste key, or A on the daisy wheel with no petal picked. Space and line ends around the
pasted text go; a field that hides its text hides what you paste too.

In a path field a paste replaces the path and shows its folder, quotes (Windows' Copy as path)
and either slash style accepted. Ctrl+V works anywhere on the chooser card, not only on the field.

## Home and Library

Home holds every console you have ROMs for and every store game and game of your own installed on
this computer, plus whatever you add to it; Library holds everything: the stores, Giveaways, the
add tiles (Add a game, Add a ROM folder, Set up <integration>), All games and one tile per console.

- After your Home comes each paired openSU's: its tile (Online or Offline), then the games on its
  Home in its order. A game already on your Home is not shown twice.
- While Home has nothing on it, as on a fresh start, it shows the add tiles too. Once a game or
  console is on Home they leave it and stay in Library.

- **Add to Home** puts any game on Home (a ROM, a game not installed yet, a game on another openSU
  on the LAN) or a console you took off. **Remove from Home** takes it off again, an installed game
  or a console too; its place in Library stays. A console opens its ROMs from Home as it does in
  Library. Both are in the tile's menu (Select or a right click) and on a game's details page.
- Consoles come first, in Library's order, then the games in the sort Quick settings picks
  (Recently played by default: installed first, then the latest played, then by name).
- A hidden game stays off Home whatever you chose; the Sort and Show filters narrow Home too.
- An installed game or a console you never took off stays on Home, and a game you install later or
  a console whose first ROM you add joins it unless you took it off before.
- Settings › Library says what Home shows. The choices are kept in `settings.json` (`home`).

### One game, many versions

Every copy of one game is one tile, wherever it comes from: ROMs on any console, a native port or a
`.desktop` shortcut, a Lutris or Wine install, and the Steam, GOG and Epic copies. Titles match when
they read the same leaving out case, punctuation and ™ (a ROM's region and disc tags never reach its
title), and "X-Men Legends II" joins "X-Men Legends II: Rise of Apocalypse" when only one game has
that subtitle. A store holds one version of a game, so two same-titled Steam games stay two.

- A game's tile carries one mark, for what runs its default version: its store (Steam, GOG,
  Epic), Linux, AppImage or Wine for a game of your own, or openSU's runner for a ROM it plays
  itself. A ROM on another emulator shows that emulator's own icon, found through its desktop
  entry or icon theme, or a Mac app's own icon, or its initials ("DS") when it has none, as an AppImage nothing names.
- A game whose default version is on another openSU carries what runs it there, ringed, and its
  title reads "· On Desk". A game here that another openSU holds too reads "· Also on Desk".
- The tile plays the game's default version: the one you made default, else an installed one before
  one that is not, the one you last played, then Steam, GOG, Epic, your own, ROMs.
- A console's shelf still lists its own ROMs, and All games lists each game once if any version is
  not a ROM.
- A game with several versions has **Versions** on its details page in place of More, each row
  named by what its version runs as: the Play of every version that plays now on top ("Play
  (PlayStation 2 · LRPS2)"), the Install of every one that does not in the middle ("Install
  (Steam)"), **Make default** for each version but the default, and each Uninstall at the bottom.
  The default is kept in `settings.json` (`versions`) by version, so a rescan does not undo it.

Settings › Controls › Shortcuts sets the key for every action (and the pad chord for those that
take one); Restore default shortcuts puts them back. Its Keyboard row picks the on-screen keyboard
the text fields open.

## The Guide menu on Home and Library

Guide or Start opens the menu; Start closes it again. Over Home and Library it holds Search among
the places. What you change in place is in Quick settings.

## Quick settings

Guide + A (Ctrl+Tab) opens a panel at the right edge, over Home or a game, which is not paused, so
a change is heard and seen as it is made. Along its top are tabs shown as icons; LB and RB step
them. Each tab holds categories, each a heading over its settings, and each setting is a Settings
row that works as it does in Settings. Until you change it there are three tabs:

- **System** (sliders): Sound (Volume, Mute), Screen (Brightness, Night light, Interface size,
  Screen), Power (Power profile), Player (Playing as).
- **View** (screen): the shelf's Sort by, Show, Installed games only, Hidden games only, Library
  layout and Icon size.
- **Device** (pad): the stick lights and the stick correction, on a ROG Ally.

A setting this machine does not have (no backlight, no power profiles) is not shown, and is kept
for another machine. **Customize** (its row, or View) arranges it:

- A grabs a tab, category or setting and A drops it; while it is held, Up and Down move a setting
  (into the next category at a category's edge) or a category, and LB and RB move a tab. B puts it
  back where it was. A tap on another place drops it there.
- X removes the focused setting, category (with its settings) or tab (with its categories).
- Y renames a category, or changes a tab's icon.
- **Add a setting** at the end of each category browses Settings and adds the row you pick; **Add
  a category** ends the tab; **+** in the tab strip adds a tab; **Done** or B leaves.

In Settings, View on a setting adds it to Quick settings (to the first tab's last category) or
takes it out; the hint at the bottom says which.

## Over a game

Devices, Downloads and Settings open from the Guide menu over it: Back on their top level returns to
the menu on the same row, over the game if one is running.

Guide over a running game opens the menu over it: Resume or Close game (B or Guide resumes), Game
settings, for a built-in core's game Save state, Load state and Reset game
([libretro.md](libretro.md)), then Devices, Downloads, Settings and Power. The game gets no
controller input while it is open.

## Calibrating a controller

Devices › Controllers has a Calibrate row under each controller. A card asks you to let go of both
sticks and both triggers for two seconds, then to turn both sticks round their edges and pull both
triggers all the way for ten. openSU then reads each stick's resting point as the centre and the
farthest it reached each way, and each trigger's farthest pull, as full travel, for that controller
by its name (and unique id) from then on: in the shell, in games here and in games streamed from
another openSU. An axis not swept at least halfway to each end is left as the controller reports
it, and the card's message names it. B on the card measures nothing; X on a calibrated
controller's row (Reset) forgets its calibration.

The card ends on Check: both sticks as circles with a dot each and both triggers as bars, live, as
games now get them, with a faint mark where the controller reads by itself. A good calibration rests
each dot in the middle, reaches the circle all round and fills each bar. Keep (or B) keeps it,
Calibrate again measures again, and Undo puts back the calibration before it, or none. Check under
each controller's Calibrate row shows the same at any time.

On a ROG Ally the card first asks whether to calibrate for the hall-effect correction too. With it,
a last step of eight seconds asks you to hold the left trigger all the way down and turn the left
stick round its edge; how far that circle sits from the one without the trigger is the trigger's
pull on the stick, which the correction then takes off for that controller even with Stick
correction off (with the correction's own deadzones then). A calibrated stick's rest is already
its centre, so Stick correction's rest offsets no longer apply to it.

## Guide chords

Chords are set on Settings › Guide chords: Quick settings, the volume, Close game, Save state, Load
state, Reset game, Devices, Downloads and Home. Hold any button and press another to set one. A
chord's buttons never reach the game. The held button still works on its own: tapped (released
within 500 ms with no chord) it reaches the game as that button; held longer it shows its chords
and, released alone, does nothing, and pressed with a button that is no chord it reaches the game
before that button. Over a game the list leaves the controller and the keyboard with the game.

## Button mappings

Button mappings change what a game gets from the controller, never how openSU itself is driven. A
game's details page has Controls for the game's own mapping; Settings › Controls has the mapping
for every game and one per emulator, shared by every game it runs. A game's own mapping replaces
its emulator's, which replaces the one for every game. Each button can send another, two together
(Select + Start) or nothing, and a mapping can add chords that send buttons or do a chord action;
they behave like the Guide chords and are listed when their button is held. With Steam Input on
(for every game, or per game), Steam has the game's controls and openSU applies no mapping to it.

On Linux a mapping reaches any game through openSU's virtual pads. On Windows, macOS and Android
mappings apply to the built-in cores' games only, inside the runner.
