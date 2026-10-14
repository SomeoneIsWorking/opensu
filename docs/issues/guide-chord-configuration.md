# Guide chords and their configuration menu

Status: open.

Requested: "Guide chord buttons with a configuration menu".

## Current state

- `input/shortcuts` is the one chord table. `PadChord` is a held modifier plus a trigger;
  `canHold` allows L2, R2, L3, R3 and Guide as modifiers. Only four actions take a pad chord
  (`takesPadChord`: QuickMenu, VolumeUp, VolumeDown, VolumeMute). The defaults are Guide + A for the
  quick menu and L2 + Up/Down/Left for the volume (`defaultPads` in `shortcuts.cpp`).
- `input/pad_chords:PadChords::feed` takes chords out of the pad stream. Guide is held until it is
  released, so a Guide tap still opens the Guide menu and Guide + button is a chord.
- `app/shortcut_editor:ShortcutEditor::capturePad` captures a new chord on Settings › Controls
  (`app/settings_controller:controlsPage`). It lists every `Action` with its key and chord. The
  listening hint reads "Press a key, or L2 + a button", even though Guide is also accepted.
  Overrides are saved as `shortcuts.pads` in `settings.json` (`settings/settings.cpp`).
- Devices › Controllers has a "Shortcut remapping" row that opens that page
  (`app/devices_controller.cpp`, `pads.remap`).
- `app/shortcut_router:ShortcutRouter::fromPad` carries out chord actions on the shell loop.

Two defects found by reading the code (not reproduced on hardware):

- **Chords leak into the game.** While a game runs, `gamepad/pads.cpp:Pads::run` writes every
  translated event to the virtual pad before the shell sees the controls.
  `PadTranslator::translate` withholds only `BTN_MODE` (Guide). So Guide + A opens the quick menu
  and also presses A in the game. L2 + Up changes the volume and also sends L2 + d-pad up to the
  game.
- **L2 and R2 chords never fire on pads with analogue triggers** (Xbox, DualSense).
  `PadTranslator::translate` reports `Button::L2/R2` controls only for digital triggers, and the
  `ABS_Z/ABS_RZ` path forwards the axis without a control. project-state S032 notes the same gap.
  The default volume chords therefore do nothing on the user's Xbox pad.

## iiSU reference

iiSU has no chord editor. Settings › Input has three single-button shortcuts, each Off/B/X/Y (enum
`ul4`): RetroAchievements popup, Screen swap and App drawer (`q52.java:56,213,274`, keys
`*_shortcut_button` in `so7.java`). L2 and R2 are fixed to the Discord and notification overlays
(`tl4.java:144-148`). L1 + R1 within 65 ms is a fixed chord (`wu3.java:91-98`). Notes are in
`docs/reference/iisu/input-sound.md` §1 and §3. The Guide-chord page is therefore openSU's own
design (a Steam Deck–style "Guide + button" list), not a replication. Record it as a delta in
project-state's baseline.

## Gap

- Only four actions take a chord. None of the in-game actions can be chorded: Close game, Save
  state, Load state, Reset game (all built for `opensu-retro` in `app/quick_menu_controller`),
  Devices, Downloads (see `downloads-page.md`), Home or Library.
- There is no Guide-chord view. The Controls page is a flat list of 19 actions, so the player
  cannot see which Guide + buttons are free.
- Chord triggers reach the game, as described above.
- Chords using an analogue L2 or R2 never fire.

## Proposed owners and design

1. `gamepad/pad_translator`: report L2 and R2 as controls from analogue triggers too, using a press
   and release threshold with hysteresis (for example 0.5 and 0.3 of full travel). This also
   closes S032's missing L2/R2.
2. `gamepad/pads` (reading thread): keep chord triggers out of the forward path. The gamepad layer
   must not include `input/`, so `Pads::hold` takes a plain `gamepad::ChordTriggers` value: the
   set of (modifier, trigger) pairs bound to something. While a modifier is held, a press of a
   trigger in that set is not forwarded, and neither is its release. The modifier itself still
   reaches the game, except Guide, which never does. The shell builds the set from `Shortcuts`
   (and, later, from the game's mapping in `per-game-button-mappings.md`) and passes it on every
   hold.
3. `input/shortcuts`: add the in-game actions as `Action`s that take a pad chord: CloseGame,
   SaveState, LoadState, ResetGame, Devices, Downloads, Home. Each gets a handler in
   `app/shortcut_router`. The retro actions go to `app/retro_session`, and do nothing (with a toast)
   when the game is not on `opensu-retro`. Defaults: keep Guide + A for the quick menu. Any other
   default Guide chord is an open question.
4. Configuration menu: a "Guide chords" sub-page of Settings › Controls, owned by a new
   `app/chord_settings.*` so `settings_controller.cpp` (580 lines) does not grow. It has one row
   per trigger button (A, B, X, Y, LB, RB, D-pad, Start, Select, L3, R3) showing the action bound to
   Guide + that button, or "None". A steps through the chord-capable actions, using
   `Shortcuts::rebind(action, PadChord)` with its existing conflict refusal. Pressing Guide plus the
   button on the pad also jumps to that row. The flat list stays for keys and for chords on other
   modifiers. Fix the listening hint so it names Guide.
5. Storage is unchanged: `ShortcutOverrides::pads` in `settings.json`.

`app/shell_app.cpp` is at 1,191 lines, which is the verifier's 1,200 limit. The new handlers must
go into `shortcut_router` or the new owner, not into `shell_app.cpp`.

## Tests

- `pad_translator`: analogue trigger edges and hysteresis.
- `pads` against uinput: a Guide + A chord forwards nothing to the virtual pad, and an L2 chord
  forwards L2 but not the trigger. This test fails before the change.
- `shortcuts`: the new actions, their spellings and conflicts.
- `chord_settings`: rows and the step-through.
- Hidden-run capture of the page under `scratch/guide-chords/`.

## Open questions

- Which Guide chords should ship by default beyond Guide + A? Steam Deck's defaults (Guide + R1
  screenshot, Guide + L1 magnifier, and so on) do not map onto openSU's actions one to one.
- Should non-Guide modifiers (L3, R3, L2, R2) stay available for shell chords at all? Each one
  costs the game a button combination.
