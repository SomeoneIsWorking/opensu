# An on-screen keyboard for everything: daisy wheel by default

Status: open. Step 1 (openSU's own fields) built: one keyboard owner with both layouts and the
Settings › Controls › Keyboard row (`docs/design/on-screen-keyboard.md`). Step 2 built: both
layouts dock along the bottom fifth, the compact wheel, anti-aliased shapes and ink-centred
characters. Step 3 built: typing into the game in front through XTest inside Gamescope, with the
preview line, opened from the Guide menu's Keyboard (also in the quick menu) or Guide + X. Still to
do: Wayland-native clients inside Gamescope, windows that are not a game launched here (the
sign-in browser), opening it when Steam or a game asks for one, layouts per language, the right
stick as a cursor, and the regular grid extended to a full layout.

The player: "I want a daisy wheel keyboard (like in Xbox) as default and a regular keyboard as an
option".

openSU's own fields type on `ui/on_screen_keyboard` (`app/keyboard_pad` for its buttons).

Decided:
- One keyboard owner with two layouts, chosen in Settings: Daisy wheel (default) and Regular (the
  existing grid, extended to a full layout).
- Daisy wheel: the left stick picks one of eight petals, each holding four characters on the face
  buttons (A/B/X/Y); triggers or bumpers switch letters, capitals, numbers and symbols; the right
  stick or d-pad moves the cursor; Start submits. Laid out per the active language.
- It types into openSU's own fields and into other apps (games, the browser). Linux, inside
  Gamescope: injected into the app in front through XTest on Gamescope's Xwayland (most games and
  the browser are X11 there); Wayland-native clients inside Gamescope to be checked. Opened with
  a Guide chord or from the quick menu, and automatically when Steam or a game asks for one if that
  request can be seen.
- Other platforms: the shell's fields only at first; Android keeps the system keyboard.

## Step 2: a compact wheel that leaves the screen visible

The player on step 1: "Daisy wheel has alignment and aliasing issues and I was thinking of
something that will also be usable without covering the screen".

Decided:
- The keyboard docks along the bottom edge, at most a fifth of the window's height, centred, on a
  translucent backing; nothing above it moves. The search panel no longer reserves room for it.
- Compact wheel: a small ring of the eight petals (each a dot with its characters in miniature) and,
  beside it, the picked petal's four characters drawn large in the A/B/X/Y diamond; the set strip
  (abc, ABC, 123, #+=) and Delete/Space hints in one row. Over a game, where openSU has no field on
  screen, a one-line preview of the text sits on the strip.
- The regular grid docks the same way, a phone-style bottom keyboard.
- Drawing: discs and rings anti-aliased (fixed in gpu2d if its rounded shapes are not), each
  character centred on its ink bounds, not its advance box and baseline.

## Step 3: typing into the game in front

Built as decided: XTest on Gamescope's Xwayland, the one owner of XTest input being
`session::XTestInput` (remote play's host input moved onto it), reached through
`app/game_typing`. While the keyboard is over a game the overlay is `OverlayMode::Typing`: drawn
without `STEAM_INPUT_FOCUS`, so Gamescope leaves the game the X keyboard focus the injected keys
are delivered by, and the game's pads rested so the face buttons that type do not also play.

Evidence (`scratch/osk-xtest/tools/type_into_app.py`, a hidden openSU in a headless Gamescope
with a GTK `zenity --entry` on X11 as its own game, all input injected over the control channel):
Guide + A opened the quick menu with Keyboard second; A on it opened the wheel with "Typing into
the game" on its preview line; five stick chords typed h, e, l, l, o; Gamescope's screenshot showed
"hello" in zenity's field (empty before) and the preview line read "hello"; Start sent Return,
zenity printed `hello` and exited 0, and openSU toasted "Typing Probe closed". Focused tests:
`keyboard_pad`, `on_screen_keyboard`, `game_screen`, `guide_menu_controller`, `shortcut_router`,
`shortcuts`, `chord_hint`, `settings_controller`, `library_tools`, `stream`.

Not shown by that run: the hidden window is never mapped, so the overlay's focus handover rests
on Gamescope's documented behaviour (`session/gamescope_overlay.hpp`), not on a frame of the
keyboard drawn over the game.
