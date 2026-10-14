# Custom button mappings per game and per emulator, with chords

Status: partial. Model, store, scopes, forward path, game chords, editor and the Steam Input
switch are built and tested; key outputs, the switch reaching Steam and the hardware checks below
are not.

Requested: "custom button mappings for games and emulators (emulated games can share config) and
again allow chord buttons".

## Built

- **Model** (`input/button_map`): a `ButtonMap` keeps what each pad button sends (`PadOutput`, a
  set of pad buttons: one, several such as Select + Start, or none) and a list of chords, each
  sending a `PadOutput` or performing a shell `Action` a chord can do (`takesPadChord`). Guide is
  openSU's: it is never mapped, never an output, and never part of a game chord. A button row
  takes pad outputs only; actions are only on chords. `forwardMap` turns a map into the pads'
  `gamepad::ForwardMap` (`gamepad/forward_map`), leaving out action chords and any chord the
  shell binds.
- **Scopes** (`input/mapping_scopes`): game, else its emulator (catalog id, from
  `app/mapping_editor:mappingKeys`), else global, else every button sends itself. Each scope's
  map is whole: a game's own map replaces its emulator's, it does not merge. The Steam Input
  switch is global plus an optional per-game choice; while it has a game, `resolve` gives no
  mapping.
- **Store** (`settings/mapping_store`): `<config>/opensu/mappings.json`
  (`{"global", "emulators": {id}, "games": {id}, "steamInput": {"global", "games"}}`), written
  whole through `fileio::writeWhole` (temp file and rename). A damaged file or entry is warned
  and left out.
- **Forward path**: at launch the shell resolves the game's map (`MappingEditor::forGame`) and
  `app/game_pads:GamePads::hold` gives the pads its `ForwardMap` and the chord set its chords.
  `PadTranslator` applies it to the virtual pad only; the shell's controls stay unmapped, so the
  Guide menu and every shell page navigate as before. A trigger mapped to a trigger keeps its
  travel. `opensu-retro` reads the virtual pad, so it gets the same mapping with
  `retro/joypad`'s one fixed table unchanged.
- **Game chords** (`input/chord_set:ChordSet`): the shell's chords plus the running game's. A
  chord both bind is the shell's. The gate's rule applies to game chords too: the modifier tapped
  under the hold delay reaches the game (as its mapped output), held it is a modifier, and a
  chord's buttons never reach the game. A pad chord's outputs are held until either button is
  released. An action chord goes through `app/shortcut_router` like a shell chord. While a game
  runs, its chords' modifiers are chord modifiers for the shell's gate too.
- **Chord list**: holding a modifier lists the shell's chords on it, then the game's ("Sends
  Select + Start" for a pad chord, the action's name otherwise) (`ui/chord_hint`).
- **Editor** (`app/mapping_editor`, `ui/mapping_page`, `ui/mapping_board_layout`,
  `ui/spatial_focus`, `ui/mapping_page_painter`): the details page's Controls button opens the
  game's own map; Settings › Controls opens the global map, an emulator's (Left and Right pick the
  emulator, A opens it), and has the global Steam Input toggle. The card is a controller board: a
  faint silhouette with a chip per pad button in its place (shoulders on top, d-pad and left stick
  left, face diamond and right stick right, Select and Start between; no Guide) and beside each,
  toward the nearest edge, what it sends: dim when it sends itself, in the accent when changed (its
  glyphs, or "Nothing"). Under it a strip: one chip per chord (`[mod] + [trigger] → output`,
  scrolled to focus), "+ Add chord", then at the right Steam Input (game page only) and the reset
  to what the scope inherits (when the scope has its own map). The direction buttons move to the
  nearest chip that way (`spatial_focus` on the design-size layout). Item ids are those of the
  control channel: `steam`, `button.<name>`, `chord.<mod>.<trigger>`, `add`, `reset`. A on a chip
  opens a picker popover beside it, flipped to stay inside the card, headed by the source glyphs
  and an arrow, its tiles in lines by group and moved over in 2D, opened on the choice in force:
  a button's outputs (face, shoulders, d-pad, sticks, Select and Start, then Nothing and Select +
  Start); a chord's (openSU's actions, the pad buttons, Select + each, Remove when it exists);
  Steam Input's three settings. B closes it. Add chord dims the board under "Hold a button, then
  press another" (`app/chord_capture`, shared with `ShortcutEditor`), B alone cancels, and Guide and
  the shell's chords are refused. The page shows the inherited map until the first change, which
  makes it the scope's own.
- **Steam Input**: while on for a game, its page says "no mapping applies; openSU's chords still
  work", and a button row or Add a chord is refused with "Steam Input has this game's controls;
  turn it off here to map its buttons". Shell chords keep working. A Steam game's page also notes
  that Steam may read the pad itself.
- **Motion**: `Pads` grabs a pad's gamepad evdev node only. A "Motion Sensors" node has no
  `BTN_SOUTH` and is never opened; hidraw is never opened. `pads_test` holds a fake motion node
  (`INPUT_PROP_ACCELEROMETER`) while the pads are held: it is not a pad, the test's own grab of it
  succeeds, and its events stay readable.

### Decisions

- Shared config is per emulator, not per system.
- A button is edited by picking from tiles, not by pressing the new output: B is both an output
  and the way back.
- A mapped chord on Start: a tap of Start under the hold delay sends Start's mapped single-press
  output, a hold plays the chord.
- A changed mapping applies the next time the shell holds the pads (the next launch).

## Not built

- **Page look**: the board is not settled with the player. Options to revisit: labels only on
  changed buttons, a two-step chord picker, a changes-only page with press-to-pick.
- **Key outputs** (a standalone emulator's keyboard hotkeys via XTest or a uinput keyboard). A
  third `ChordOutput` alternative would carry them; nothing else in the model changes.
- **How the switch reaches Steam.** openSU stores the switch and applies its own side (no mapping
  while on). Still to measure on a running client: whether Steam honours the per-app
  `UseSteamControllerConfig` in `localconfig.vdf` or needs a `SteamClient` call, and whether
  handing Steam openSU's virtual pad is workable (Steam would also see the hidraw pad: doubled
  input).
- A quick menu Controls row for editing in game, and a live re-apply while the pads are held.
- Pointer input on the mapping page (pad and keyboard only).
- A `runner_test` run of `opensu-retro` against a remapped virtual pad; `pads_test` covers the
  virtual pad it reads.

## iiSU reference

iiSU has no button remapping. The dex has no `input_player*`, `.rmp`, "controller profile" or
"button layout" strings. It reads `retroarch.cfg` only to find `libretro_directory` (`qo3.z`). Its
only input mapping is the global "Swap A/B and X/Y" (`swap_abxy`, `jj2.V`, `so7.java:552`) plus
"Swap A/B glyphs". It has a per-console emulator and core override (`lm7.java`, `kp3.java`). This
card is therefore openSU's own design. iiSU's swap A/B maps onto a global profile with A↔B and X↔Y
swapped.

## Motion sensors: measurement still needed

Emulators with gyro support (Cemu, Dolphin, Eden, Citra and others) must still get the real pad's
motion. `pads_test` shows a motion node is never grabbed and stays readable while the pads are
held; whether an emulator then reads it has not been seen. To measure on a DualSense or Switch
Pro: run an emulator under openSU with the pad held and check its gyro input. Measure at the same
time whether SDL's HIDAPI drivers, which read hidraw directly, let an SDL emulator see the real
pad's buttons too (doubled input, and the mapping bypassed). If so, the fix belongs here:
`SDL_JOYSTICK_HIDAPI=0` in the game's environment (motion then comes from the evdev motion node),
or a virtual pad that carries motion.

## Tests

- `button_map`: what a map takes and refuses, names, `forwardMap` (shell chords left out), scope
  resolution order, whole-map override, Steam Input global and per game.
- `mapping_store`: round trip, missing and damaged file, damaged entries left out, a save into a
  folder that cannot be written fails and leaves the old file whole with nothing beside it.
- `mapping_editor`: a game's page (its item ids, every item and picker tile reachable by the
  direction buttons), a pick kept on disk, an emulator's map shared and copied into a game's own,
  a chord heard then given an output or an action or removed, Guide and shell chords refused,
  Steam Input refusing and dropping the mapping, Settings › Controls rows.
- `mapping_page`: spatial focus (straight ahead before diagonal, the cone), moves over the board
  and strip, the strip scrolled to a chord with Reset kept apart, every chip and callout on the
  board without overlaps at three frame sizes, the picker's lines and 2D moves, the popover inside
  the card for every item and picker with the focused tile in view, and beside A's chip.
- `pad_translator`: a mapped button, a mapped trigger's travel, Guide never mapped, a game chord
  holding its outputs, a mapped Start tap.
- `pads` (uinput): a remapped A reaches the virtual pad as B while the shell hears A; a motion
  node is never grabbed.
- `shortcuts`, `chord_hint`: game chords join the shell's, the list shows them.
- `settings_controller`: the Controls page's mapping rows.
- Parity scenes `game-mapping`, `mapping-picker`, `mapping-listening`, `chord-picker`,
  `global-mapping`.

## Only on hardware

- A real pad remapped in a native game, an SDL game and a standalone emulator.
- A Steam game whose pad Steam reads through hidraw (the mapping does not reach it).
- Gyro in Cemu, Dolphin or Eden with the pad held (the motion node above).
- A Start tap in a game that polls.
- Doubled input from SDL's HIDAPI drivers reading hidraw in an emulator.
