# Guide opens Steam's Big Picture, and openSU keeps acting behind it

Status: partial. Fixed and unit-tested through the shipping code; not yet seen on the real pad in
the real Gamescope session.

## Observed

On the player's machine (Xbox Series pad on the xone driver: one evdev node and `js0`, no hidraw),
inside openSU's Gamescope with Steam started:

- Guide in the shell opened Steam's Big Picture.
- With Big Picture in front, openSU still navigated and played its sounds on every press.
- From Big Picture there was no way back to openSU, and openSU's overlay did not show over it.
- After closing Big Picture with Alt+F4, Guide in the shell brought up Steam's regular window.
- The player does not want Big Picture at all.

## Cause

- `gamepad/pads:Pads` grabbed the pads (EVIOCGRAB) only inside `hold()`, while a game ran. In the
  shell the pads were read ungrabbed, so the Steam client openSU starts (`steam -silent`) read them
  too and answered Guide with Big Picture, or with its main window once Big Picture was gone. A
  grab on the evdev node also keeps the pad from joydev (`js0`), which is another handler of the
  same device.
- `app/shell_app:ShellApp::handlePads` acted on every pad event whatever Gamescope had focused.
- Gamescope (the pinned fork, not run with `-e`) gives every window an app id equal to its own X
  id, so focus goes to the most recently mapped window (`is_focus_priority_greater`, map sequence).
  Big Picture, mapped last, took the focus. openSU's window is Gamescope's overlay only while a
  game runs (`STEAM_OVERLAY`); in the shell it is an ordinary window, so its Guide menu was drawn
  in a window behind Big Picture. `GAMESCOPECTRL_BASELAYER_WINDOW` cannot bring it back: the
  focus-control window is honoured only by the SteamControlled connector strategy; under the
  default SingleApplication every window's connector key is 0, so the first candidate wins as
  before (`pick_primary_focus_and_override`). `GAMESCOPE_FOCUSED_APP` is published only in Steam
  mode; `GAMESCOPE_FOCUSED_WINDOW` and `GAMESCOPE_FOCUSABLE_WINDOWS` are published always.

## Fix

- `gamepad/pads:Claim` says who else sees the pads: Free (everyone), Shell (grabbed, no virtual
  pads), Game (grabbed, the game's virtual pads), Away (a game runs under another window: the real
  pads are everyone's, the virtual pads rest). One worker applies it per pad, and a pad connecting
  later takes the claim in force.
- `session/window_focus` reads Gamescope's focus: `parseFocusable`, `describeFocus` and
  `holderOf` (openSU, the launched game, Big Picture, another window) are pure;
  `session/gamescope_windows:GamescopeWindows::takeChange` watches the root's two properties.
  Big Picture is the window whose app id is 769, which Gamescope gives a window carrying
  `STEAM_BIGPICTURE` (steamcompmgr.cpp:5838, 6930-6939); Steam's binary names that property.
- `app/pad_owner:PadOwner` (was `GamePads`) turns the situation into a claim (`claimFor`): inside
  Gamescope the shell in front holds the pads; any other window in front gets them and the shell
  hears only Guide and controllers coming and going (`admit`); a game in front holds them as
  before, and a menu over the game is openSU's whatever is focused under it.
- `app/focus_follow:FocusFollow` closes Big Picture whenever it takes the focus (by Guide, a
  `steam://` link or Steam itself) through `steam/big_picture:BigPicture::close`, which calls
  `SteamClient.UI.ExitBigPictureMode()` over DevTools (what Big Picture's own window calls on close,
  `steamui/chunk~2dcc5aaf7.js`), and puts the shell back in front. Guide pressed while another
  window is in front of the shell brings the shell back (`guideReturns`); over a game Guide opens
  the menu as the overlay, which Gamescope paints over whatever is focused.
- `app/game_screen:GameScreen::takeFocus` maps the shell's window again so Gamescope focuses it;
  over a running game it does nothing (the game gets the focus back once Big Picture is gone).
- Outside Gamescope there is no focus to follow and no Steam to keep the pads from inside the
  session, so the shell leaves the pads free as before; a game still holds them. A hidden run reads
  an empty directory and keeps no pads.

No change to the fork is needed. If openSU ever wants focus by property instead of remapping, the
fork would have to honour `GAMESCOPECTRL_BASELAYER_WINDOW` under the SingleApplication strategy in
`pick_primary_focus_and_override`.

## Tests

- `pads` (uinput pads, read through a private directory of links, so no other controller is opened
  or grabbed): under Shell another reader cannot grab the pad and gets no Guide while openSU hears
  it; Free gives it back; Away gives the pad to another reader, rests the game's stick and keeps
  presses from the game; back to Game, the game has the stick and nothing else reads the pad.
  Mutating Shell to not grab, or Away to stay live, fails it.
- `window_focus`: the triples, the focused window's pid and app id, and the holder for openSU's
  window, its pid, Big Picture, a Steam dialog, the game's pids, and unlisted windows.
- `focus_owner`: the claim table, the shell hearing only Guide behind another window, Big Picture
  closed once per focus and the shell put back, Guide's return rule.
- `steam_devtools`: the close calls `SteamClient.UI.ExitBigPictureMode()`; an unreachable Steam
  reports failure.

## Only hardware can confirm

- Guide in the shell no longer reaches Steam, and nothing opens.
- Steam really sets `STEAM_BIGPICTURE` on the Big Picture window it opens outside `-gamepadui`, so
  it is detected and closed; and leaving Big Picture this way does not bring up Steam's main
  window.
- Remapping the shell's window gives it the focus back in the real session.
- A Steam dialog (the uninstall wizard) gets the pad while it is in front, and Guide brings openSU
  back from it.

## Known side effect, not changed

Big Picture's power menu offers "Switch to Desktop" because the player's own
`/usr/bin/steamos-session-select` exists (from their fedora-kde-steamdeck setup) and Gamescope sets
`XDG_CURRENT_DESKTOP=gamescope`. With Big Picture closed on sight this menu is rarely reachable.
