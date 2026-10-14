# Custom button mappings per game and per emulator, with chords

Status: open.

Requested: "custom button mappings for games and emulators (emulated games can share config) and
again allow chord buttons".

## Current state

- **Native games** read one uinput Xbox 360 pad (045e:028e) per physical pad
  (`gamepad/virtual_pad`). `gamepad/pad_translator:PadTranslator::translate` fills it with a fixed
  code-for-code mapping (`forwardedKeys`, axes rescaled, button d-pad turned into the hat). Nothing
  can be remapped. SDL games get `SDL_GAMECONTROLLER_IGNORE_DEVICES_EXCEPT=0x045e/0x028e` from
  `Pads::hold`.
- **Built-in emulator games** (`opensu-retro`) read every pad without grabbing, the shell's virtual
  pad included (`retro/runner_app.cpp:openPads`, `OwnVirtualPads::Read`). While the shell holds the
  pads, the physical ones are grabbed, so the runner only sees the virtual pads.
  `retro/joypad:Joypad` maps `gamepad::Button` to `RETRO_DEVICE_ID_JOYPAD_*` with one fixed table
  (`buttonIds`, Xbox positions) and a fixed keyboard table. `docs/design/libretro.md` §1 states "one
  controller mapping for every core" as the goal, so this card changes that goal. Every pad is
  merged into port 0, and there are no analogue sticks (S032 gaps).
- **Standalone emulators** (`library/emulators`, `assets/emulators.json`) read the virtual pad
  through their own input config. The catalog knows each emulator's settings folder
  (`EmulatorEntry::config`), but openSU never writes it.
- **Steam games** get Steam's environment, not the SDL hint. Steam Input reads hidraw where a pad
  has it, and a grab does not cover hidraw (project-state S023 to S025 gaps). An xone pad has no
  hidraw, so Steam reads the virtual pad.
- The precedent for "a game's own setting, else a shared one" is
  `library/emulator_choice:EmulatorChoices`, which picks a game's emulator, else its system's, and
  keeps both in `settings.json`.
- Shell chords exist (`input/shortcuts`, `input/pad_chords`; see `guide-chord-configuration.md`).
  Game chords do not.

## iiSU reference

iiSU has no button remapping. The dex has no `input_player*`, `.rmp`, "controller profile" or
"button layout" strings. It reads `retroarch.cfg` only to find `libretro_directory` (`qo3.z`). Its
only input mapping is the global "Swap A/B and X/Y" (`swap_abxy`, `jj2.V`, `so7.java:552`) plus
"Swap A/B glyphs". It has a per-console emulator and core override (`lm7.java`, `kp3.java`). This
card is therefore openSU's own design. iiSU's swap A/B maps onto a global profile with A↔B and X↔Y
swapped.

## Gap

There is no mapping model, no store, no scope resolution, no editor, and no place where a mapping is
applied. Game chords (a held modifier plus a button producing an output for the game) do not exist.

## Proposed owners and design

### Data model (`input/button_map.*`)

- A `ButtonMap` has the following parts:
  - `buttons`: a map from a physical `gamepad::Button` to an `Output`.
  - `chords`: a list of `{PadChord, Output}`.
  - Later, `swapSticks` and `invertY` flags.
- An `Output` is one of:
  - a virtual pad button (or none, which disables the button);
  - a set of virtual pad buttons pressed together, so one chord can send an emulator hotkey such as
    Select + Start;
  - a key with modifiers (`input::Combo`), for a standalone emulator's keyboard hotkeys;
  - a shell `Action` (Save state, Load state, Close game, and so on).
- The mapping is expressed in the controls `Event` already names, so it stays independent of
  device and core.

### Scopes and "emulated games share config" (`input/mapping_scopes.*`)

Resolution is: game override, else emulator profile, else global profile, else identity.

- The game key is `Game::id`.
- The emulator key is the catalog id of the emulator the ROM launches with (`Game::emulator` after
  `EmulatorChoices::apply`). Every game that runs on PCSX2, or on the built-in mGBA, shares that
  profile. A per-game override replaces the whole map, not single buttons, so what applies is
  always one visible map.
- Native store games have only the game and global scopes.
- One `ButtonMap` per scope, stored separately from the shell's shortcut table.

### Store (`settings/mapping_store.*`)

`<config>/opensu/mappings.json`, written atomically like `settings::Store`, with keys
`global`, `emulators.<id>` and `games.<id>`. It is kept out of `settings.json` because profiles grow
per game.

### Where a mapping is applied

1. **Every game the shell holds pads for** (native, standalone emulators, `opensu-retro`):
   `Pads::hold` takes a `gamepad::ForwardMap`. This is a plain struct in the gamepad layer, made
   from the resolved `ButtonMap` by the shell (it holds the button remaps plus the chord triggers
   from `guide-chord-configuration.md`). `PadTranslator` applies it on the forward path only. The
   controls the shell reads stay unmapped, so Guide-menu navigation never changes. A button
   output, or a set of buttons, is written to the virtual pad. A chord's trigger is held back.
   Because `opensu-retro` reads the virtual pad, one mechanism covers built-in games, with no
   second mapping in `retro/joypad`.
2. **Key outputs** (standalone emulator hotkeys such as F1 for a save state) are injected into the
   game's X display with XTest by a new `session/game_key_injector.*`. It sits beside
   `session/game_keys`, which already opens that display with XInput2. This needs a measurement
   under nested and session Gamescope first. The alternative is a second uinput device, a keyboard,
   which works without X but is seen by the desktop in nested mode.
3. **Action outputs** go to `app/shortcut_router` on the loop. Retro state actions go through
   `app/retro_session`.
4. **Steam games** get the virtual pad mapping only where Steam reads the virtual pad (no hidraw).
   Where Steam Input reads hidraw, Steam's own per-game configuration wins and openSU cannot
   override it without running Steam's configurator, which is a Steam window the baseline forbids
   ("No store client window is ever opened"). The editor must say so for Steam games. Hiding
   hidraw from Steam would need a udev rule (root) and is out of scope.

### Editor (`app/mapping_editor.*`, `ui/mapping_page.*` with its painter)

- It opens from the details page as a new `DetailsAction::Controls`, for the game's own map or, on
  a ROM, its emulator's. It also opens from Settings › Controls for the global profile, and from
  Settings › Emulators for each emulator's profile.
- It has one row per physical button showing its output. A captures the next button press as the
  new output, mirroring `ShortcutEditor`. Chords are added by holding a modifier and pressing a
  button, then picking the output from a list.
- Key outputs are typed on the existing on-screen keyboard walk.
- "Reset to emulator profile" and "Reset to global" are offered.
- While a game runs, the quick menu gains a "Controls" row so a mapping can be edited in game. It
  applies on the next `hold`. A live re-apply needs `Pads` to accept a new map while held.

## Tests

- `button_map` and `mapping_scopes` unit tests: resolution order and that an override is a whole
  map.
- `mapping_store`: round trip and corrupt-file defaults.
- `pad_translator` with a `ForwardMap`: a remapped forward, the controls left unmapped, a chord
  output, and a held-back trigger.
- `pads` against uinput: a remapped A reaches the virtual pad as B.
- `runner_test`: `opensu-retro` reading a remapped virtual pad.

## Open questions

- Should "share config" be per emulator (proposed) or per system? It matters for multi-system
  emulators: built-in mGBA covers GB, GBC and GBA, and Mesen covers NES and SNES. Per system would
  add a scope between the game and the emulator.
- Should the global profile apply to native store games, or only to emulated ones?
- Does Steam Input's coexistence on hidraw pads (DualSense, Switch Pro) need solving now or only
  documenting?
