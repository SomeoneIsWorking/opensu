# Controls and shortcuts

Controllers are read from evdev on Linux: any pad whose driver follows the kernel's gamepad codes
(xpad, xone, hid-playstation, hid-nintendo, ...) works, keyboards are never taken for one, and a
pad switched on while openSU runs is picked up. Elsewhere pads come through SDL. The mouse works
everywhere, and on Android touch: a click or tap focuses and presses.

## In the shell

| Pad | Keyboard | Action |
| --- | --- | --- |
| D-pad or left stick | arrows, WASD | Move focus |
| A | Enter, Space, click | Open a game's details page; Play, Install, change a ROM's emulator or Hide from there |
| B | Escape | Back |
| Y | Y | Details page |
| X | F | Refresh the library |
| Select | Tab, Menu key, Shift+F10, right click | The focused tile's menu |
| Start | E | Menu: Library's layout (Standard, XMB or Carousel), icon size and sort |
| LB / RB | `[` / `]` (R for next) | Previous / next section: Home, Library |
| | `/`, Ctrl+F | Search across every store |
| Guide | Shift+Tab | The Guide menu: Home, Library, Devices, Downloads, Settings, Power |
| Guide + A | Ctrl+Tab | Quick menu (the Guide menu on Volume), over a game too |
| Guide + Up / Down / Left | Ctrl+Up / Ctrl+Down / Ctrl+M | Volume up / down / mute |
| | Q | Quit openSU |
| Hold Guide (or any chord's held button) | | After half a second, a list of that button's chords, also over a game in session mode; it goes when you let go or play a chord |

## Home and Library

Home holds every store game and game of your own installed on this computer, plus whatever you
add to it; Library holds everything: the stores, Giveaways, the add tiles (Add a game, Add a ROM
folder, Set up <integration>), All games and one tile per console.

- While Home has nothing on it, as on a fresh start, it shows the add tiles too. Once a game or
  console is on Home they leave it and stay in Library.

- **Add to Home** puts any game on Home (a ROM, a game not installed yet, a game on another openSU
  on the LAN) or a console, which opens its ROMs from Home as it does in Library. **Remove from
  Home** takes it off again, an installed game too; its place in Library stays. Both are in the
  tile's menu (Select or a right click) and on a game's details page.
- Consoles you added come first, in Library's order, then the games in the sort the Start menu
  picks (Recently played by default: installed first, then the latest played, then by name).
- A hidden game stays off Home whatever you chose; the Start menu's filters narrow Home too.
- An installed game you never took off stays on Home, and a game you install later joins it unless
  you took it off before.
- Settings › Library says what Home shows. The choices are kept in `settings.json` (`home`).

Settings › Controls › Shortcuts sets the key for every action (and the pad chord for those that
take one); Restore default shortcuts puts them back. Its Keyboard row picks the on-screen keyboard
the text fields open.

## Over a game

Guide over a running game opens the menu over it: Resume or Close game (B or Guide resumes), Game
settings, for a built-in core's game Save state, Load state and Reset game
([libretro.md](libretro.md)), then Devices, Downloads, Settings and Power. The game gets no
controller input while it is open.

## Guide chords

Chords are set on Settings › Guide chords: Quick menu, the volume, Close game, Save state, Load
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
