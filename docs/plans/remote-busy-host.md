# A paired openSU that is playing something

Asked 2026-11-18: remote play said "fedora already running Legend of Zelda" and "you are completely
locked out, you have no way of seeing what is running on fedora and you can't interact with it,
this needs its own system to handle, show running game, resume it / take over from remote etc or
close it options".

## Now

The host refuses `/lan/play` for any other game while one runs (`RemotePlay::offer`: "it is playing
<title> now"), and the guest shows that as a toast. Taking over exists: asking for the game that
runs, from a device whose active profile is linked with the profile playing it, streams it with no
question (`RemotePlay::continueSeat`), and its details page reads "Continue here". Nothing leads the
player there: the running game is listed (`ListedGame::playingAs`) but not shown, and nothing closes
it.

## What it does

- **Shown.** A paired openSU playing a game says so where its games are: its launcher tile and its
  Home section read "Playing <title>", and that game's tile stands first in the section with a
  Playing mark.
- **Asked, not refused.** Playing another game on a host that is busy opens a question instead of a
  toast: "<host> is playing <title>", with **Continue it here** (the take-over above, when the
  profile is linked with the one playing it), **Close it and play <game>** and **Cancel**.
- **Closing from afar.** `/lan/close` closes the game running there for a paired openSU. A game on a
  built-in core is saved and closed (its resume state, as Save and close does) so nothing is lost; any
  other game is closed as Close game does, and the question says what is not saved is lost. A
  device whose profile is linked with the one playing closes it with no question; any other asks the
  host's player first, Decline focused, as joining does. A guest streaming it is told "<host> closed
  the game".
- What runs there is read from the listing as it is: the listed game whose `playingAs` is not empty
  (sent since 2026-11-05), so it works with a host not updated since.

## Owners

- `lan/wire`: `CloseRequest` and its verdict.
- `app/remote_play`: `close` decides and asks.
- `app/shell_app` with the game's runner owners: closing the running game for a peer, through the
  same path as the Guide's Close game and Save and close.
- `lan/lan_node`: `askToClose`, polled as `askToPlay` is.
- The client: the launcher and Home section line, the Playing mark, and the busy question
  (`ui/game_details` launch, a dialog on the details controller).

## Order

1. The client shows "Playing <title>" from `playingAs`, and the question with Continue it here (or
   Join it, for a profile not playing it, which asks the host's player) and Cancel. Shown with two hidden openSUs (`tools.nat_lab`).
2. `/lan/close` with Save and close for a built-in core and Close game otherwise, the linked-profile
   rule and the host's question; Close it and play. Shown the same way, then from the tablet.

## State

Step 1 built: the launcher line (`library::playingOn`, `Launcher::playing`) and the question
(`ShellApp::askBusyHost`), shown by `tools.nat_lab lan-busy` (the host plays a Game Boy ROM, the
friend asks it for the GBA one). Step 2, closing from afar, is not built.
