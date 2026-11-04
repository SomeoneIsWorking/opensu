# LAN

With LAN on, openSU finds the other openSUs on your network, pairs with the ones you accept,
lists their installed games in your Library under their names, can keep your saves the same on
both, plays their games here: the game runs over there and streams to this screen (remote
play), and copies their ROMs into this openSU, so a phone can play the games on your PC.

LAN is an integration, off until you turn it on in Settings › Integrations
([stores.md](stores.md#integrations)). Finding other openSUs works on Linux with Avahi running
(`avahi-daemon`, on by default on most desktops) and on Android over Wi-Fi. On Windows and macOS,
Settings › LAN says it is not built yet.

## Pairing

1. Turn LAN on on both openSUs.
2. On one of them, open Settings › LAN. The other one is listed under *Found here*; press Pair.
3. Both screens show the same six-digit code. On the other openSU, a question asks whether to
   pair. Accept only if it shows the code you see here; Decline is the default.

Once paired, each openSU lists the games installed on the other one. In Library they are a
launcher named after that openSU, with its covers. They are not on Home unless you add one
([Home and Library](controls.md#home-and-library)). They are listed
while that openSU is on and on the network, and are checked again every minute.

## Settings › LAN

- **Name**: how other openSUs list this one. It is the computer's name until you change it.
- **Found on the LAN**: whether others can find this openSU, and the port it listens on (7312
  unless `OPENSU_LAN_PORT` says otherwise; [environment.md](environment.md)). Allow that port in
  your firewall for other machines to reach it.
- **Found here**: other openSUs on the network you have not paired with.
- **Paired**: the ones you have. Remove forgets the pairing here and tells the other openSU to
  forget it too. Its games leave Library on both sides. Pairing again shows a new code.
- **Share saves with <name>**: under each paired openSU, off until you turn it on. Saves are
  shared only when both openSUs have it on for each other.
- **ROMs on <name>**: under each paired openSU, its ROMs to copy here
  ([Importing ROMs](#importing-roms)).

## Importing ROMs

1. Open Settings › LAN › **ROMs on <name>**. Its consoles are listed with how many games each has
   and their size.
2. Open a console and pick games, or **Whole console**. Back shows how many are picked.
3. When that openSU has a BIOS file the emulator here lacks for those consoles, it is listed under
   **BIOS**. One the emulator needs is on; turn on any other you want.
4. Press **Import**. The games go into the first ROM folder this openSU can write, in the folder it
   already has for that console, else one named as on the other openSU. With no such folder,
   openSU asks you to pick one (on Android, the system's folder picker); it is added to your ROM
   folders.

Each game shows its progress and the total under **Importing**. A game appears in your Library as
soon as it arrives. Each file is checked against the other openSU's copy before it is kept.

- If the connection drops or the other openSU closes, the game waits and goes on from where it
  stopped once it is back. **Stop importing** drops what has not arrived.
- Only the files the other openSU lists in its ROM folders, and its BIOS files, can be copied.
  Nothing else on that computer can be asked for.
- An Android openSU can import, but does not offer ROMs from a folder picked through the system
  picker.

## Shared saves

For a game installed on both openSUs, the saves of the emulator it starts with follow you:

- Before the game starts, openSU takes the other one's saves if they are newer ("Syncing ...'s
  saves first").
- After the game ends, the other openSU takes yours.
- The game's details page says how its saves stand under **Saves**: In sync with Desk, Newer on
  Desk, Conflict with Desk, or Desk is offline. **Sync saves now** syncs at once.
- If you played on both since they last synced, nothing is replaced. The row says **Pick which
  save to keep** and shows each side's date and size. B cancels.
- The saves replaced are always backed up first, as Copy saves does, under
  `~/.local/state/opensu/save-backups/<console>/<game>/<emulator>/<time>/`.
- Nothing is replaced while that emulator runs. Close it and sync again.

Switch games match across emulators by title, so Eden on one openSU and Ryujinx on the other share
a save. Other games match by ROM name on the built-in emulators.

## Remote play

Press Play on another openSU's game in Library. That openSU starts the game and sends its picture
and sound here, and your controller, keyboard and mouse play it.

- Both openSUs run on Linux. The one with the game must run as a session in Gamescope (openSU's
  own session, or `opensu` started inside `gamescope`); otherwise it says it cannot stream.
- Use a wired gigabit network: the picture takes around 50 Mbit/s for simple games and up to
  200 Mbit/s for busy ones. Wi-Fi is not supported yet.
- Allow UDP in your firewall between the two computers: each session uses a free UDP port on the
  openSU that has the game.
- Guide opens this openSU's menu. **End remote play** closes the game over there; anything it has
  not saved is lost.
- The other openSU runs one game at a time. While you play its game there, or its own game runs,
  it refuses another.
- Triggers are on or off, not pressure-sensitive, and the game uses the default controls, not a
  mapping you made for it.

## What is shared, and with whom

- An openSU you have not paired with learns only this one's name. Everything else is refused.
- A paired openSU sees the titles and covers of your installed games, the ROMs in your ROM
  folders and the BIOS files for their consoles (and can copy them), and the saves of the games
  you share saves for, nothing more. It can start one of those games here to play it remotely;
  this screen then shows a notice naming it.
- Pairings are kept in your desktop's keyring when there is one, otherwise in an owner-only file
  in openSU's config folder.
- What is sent between paired openSUs, remote play's picture, sound and input included, is
  encrypted and signed with the pairing's secret. Someone
  else on the network can see that two openSUs talk and how much, but not the games or saves, and
  cannot ask in a paired openSU's name or change its answers.
- Accept a pairing only when both screens show the same code. That check is what keeps someone
  else on the network out of the middle.
- The two computers' clocks must agree to within two minutes.
