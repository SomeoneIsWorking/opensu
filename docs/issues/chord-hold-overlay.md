# Holding a chord button shows that button's chords

Status: open.

Requested: "holding a chord button can show mappings for that chord button".

## Current state

- `input/pad_chords:PadChords::feed` knows when a modifier is held (`held_`) and holds Guide back
  until it is released (`guide_`). It has no notion of time. Holding Guide and letting go without a
  chord comes out as a tap, which opens the Guide menu.
- `input/shortcuts:Shortcuts::padChord` answers per action. There is no "every chord on this
  modifier" query.
- Over a game, the shell draws only through the Gamescope overlay. `app/game_screen` shows the
  window while a menu or page is up, and `session/gamescope_overlay:setShown` sets
  `STEAM_INPUT_FOCUS`, which hands it the keyboard. Opening a menu also blocks the pads
  (`Pads::setBlocked`, which rests the virtual pads).
- The direction-repeat timing (400 ms, `gamepad/direction_repeat`) is the only hold timing in the
  input layer.

## iiSU reference

None. `docs/reference/iisu/input-sound.md` records that no handler implements a hold or long press.
The only hold is A to pick up a tile in reorder mode (`l80.java:357`). The closest feature is the
static "Show Button Hint" setting (`dual_detail_button_hint`). The model for this card is SteamOS,
where holding the STEAM button shows the button-chord list. That model has not been checked on a
Deck here.

## Gap

Nothing detects a held modifier past a delay. There is no chord list model, no hint painter, and no
way to show the overlay over a game without taking focus and blocking the pads.

## Proposed owners and design

1. `input/pad_chords`: `feed` takes the frame's time. When a modifier has been held for 500 ms with
   no trigger, it reports `hintShown{modifier}`, then `hintHidden` on release. Guide released after
   the hint was shown is not a tap, so the Guide menu does not open (as on SteamOS). A chord made
   while the hint is up works as usual and closes the hint. The logic is pure and is tested with
   synthetic times.
2. `input/shortcuts`: `chordsOn(modifier)` returns each trigger with its action label. When the
   running game has a mapping (`per-game-button-mappings.md`), `input/mapping_scopes` adds its
   chords for that modifier, with the output's name.
3. `ui/chord_hint.*` and `chord_hint_painter.*`: a compact panel listing trigger glyph → label, in
   the button glyphs of `ui/button_glyph`. Over the home screen it draws like the toast. Over a
   game it goes through `GameScreen`.
4. `app/game_screen` and `session/gamescope_overlay`: add a "shown without focus" state that maps
   the overlay without `STEAM_INPUT_FOCUS` and without `Pads::setBlocked`. The trigger press has to
   reach the chord matcher, and every other button has to keep reaching the game.
5. Wiring is in `app/shortcut_router`, which already owns `PadChords`. `shell_app.cpp` is at its
   line limit.

## Tests

- `pad_chords`: the hint after the delay, no hint for a quick chord, Guide hold-and-release not
  being a tap, and the hint closing on a chord.
- `shortcuts`: `chordsOn`.
- A hidden-run `--render` of the hint.
- An overlay check that the pads stay unblocked while the hint shows (`pads` against uinput).

## Depends on

`guide-chord-configuration.md`: there are chords worth listing, and analogue L2 and R2 work as
modifiers. `per-game-button-mappings.md` is needed for game chords in the list.

## Open questions

- Is 500 ms the right delay? SteamOS's is not measured here.
- Should a long Guide hold followed by a release open nothing (proposed, as on SteamOS) or still
  open the Guide menu?
