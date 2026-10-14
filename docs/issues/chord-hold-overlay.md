# Holding a chord button shows that button's chords

Status: partial. Built and verified in hidden runs; the list over a real game in Gamescope and on a
real pad has not been seen.

Requested: "holding a chord button can show mappings for that chord button".

## Behaviour

- A modifier (a button some bound chord starts with; Guide by default) held alone for the hold
  delay (`input::holdDelay`, 500 ms) shows a panel centred on the screen: the modifier's glyph,
  "Hold and press", and one row per chord on it, the partner's glyph and the action's name.
- It goes when the modifier is released, when a chord is played (the chord still plays), or when
  another button lets the modifier out. It fades in 110 ms and out 160 ms.
- Released after the long hold with no chord, the modifier does nothing (the gate's existing rule),
  so a long Guide hold opens no Guide menu.
- Over a game inside Gamescope the overlay is passive: drawn, with `STEAM_INPUT_FOCUS` 0 and the
  pads not blocked, so the game keeps the keyboard, the pointer and every button that is not part
  of a chord. A menu or page is interactive, as before. Outside Gamescope the window would take the
  focus if mapped, so the list is not shown over a game there.

## iiSU reference

None. `docs/reference/iisu/input-sound.md` records that no handler implements a hold or long press.
The model is SteamOS, where holding the STEAM button shows the chord list. That model has not been
checked on a Deck here, and the 500 ms delay is ours.

## Owners

- `gamepad/chord_gate:ChordGate::tick` reports a modifier held alone for the hold delay once
  (`Step::LongHold`); `press` and `release` report its end once (`Step::LongHoldEnded`) on a chord,
  a let-out or a release. The timing is the gate's alone; `PadTranslator` ignores both steps.
- `input/pad_chords:PadChords::longHeld(at)` ticks every pad's gate and answers the modifier long
  held now; `app/shortcut_router` passes it on.
- `input/shortcuts:Shortcuts::chordsOn(modifier)` lists the chords on a modifier, and `glyphOf`
  names each button's glyph key (`buttonOfGlyph` is its inverse).
- `ui/chord_hint` is the model (`chordHintRows`, `follow`, the fade); `ui/chord_hint_painter` draws
  it in the volume display's colours. `ui/button_glyph` gained LT and RT (the shoulder shape), d-pad
  arrows in the letter ring, and a named cap for the rest (L3, R3, GUIDE).
- `session/gamescope_overlay:OverlayMode` (hidden, passive, interactive) and
  `app/game_screen:GameScreen::show`, which also owns resting the game's pads: only an interactive
  overlay blocks them. `ShellApp::syncOverlay` asks for interactive while a panel is open, passive
  while the list is visible, else hidden.
- `POST /input guide 3000ms` holds buttons for a duration (`app/control_bridge` keeps every later
  edge queued behind the release), and `/state` reports `chordHint` ("guide" or empty).
- To keep `shell_app.cpp` from growing, the clock and battery tick moved out into
  `app/top_bar_clock`.

Game chords from per-game mappings (`per-game-button-mappings.md`) are not listed; that card adds
them to `chordsOn`'s source when it lands.

## Evidence

- Fail first: the new tests do not compile against d6ff921 (no `Step::Kind::LongHold`,
  `ChordGate::tick`, `PadChords::longHeld`, `Shortcuts::chordsOn`, `glyphOf`, `ui/chord_hint`,
  `session::OverlayMode`, `GameScreen::show`, or a hold on `ControlTarget::inject`); there was no
  hold behaviour to fail at run time.
- Unit tests, all passing: `chord_gate_test` (a long hold reported once at the delay, ended by a
  release, a chord or a let-out, none for a quick chord), `shortcuts_test` (`chordsOn` lists exactly
  a modifier's chords after a remap; `longHeld` until a chord or release; a long Guide release is
  no Guide press), `chord_hint_test` (rows are exactly the modifier's chords with glyph and label;
  shown and faded with the hold), `game_screen_test` (passive leaves the pads unblocked, a menu
  blocks them, closing it under the list unblocks them), `control_bridge_test` and
  `control_channel_test` (a hold keeps the release, and every later edge, back; holds over 10000 ms
  or in other units are refused; `chordHint` in `/state`).
- Hidden run (`scratch/chord-hint/verify.py`): `guide 2500ms` shows `chordHint: "guide"` 0.56 s
  after the post (none at 0.25 s), hides it on release, and the Guide menu stays closed; a Guide tap
  still opens the Guide menu; `guide+a 1500ms` opens the quick menu with no list at any point.
- Parity scene `chord-hint` in `tools/screenshots`: the panel centred over Home, clear of the top
  bar, dock and hints.

## Left

- On hardware, inside Gamescope over a running game: the list shows, the game keeps its input
  (keyboard, pointer and the buttons that are not part of a chord), and a chord played under it
  still works.
- Whether 500 ms suits a real pad (SteamOS's delay is not measured here).
