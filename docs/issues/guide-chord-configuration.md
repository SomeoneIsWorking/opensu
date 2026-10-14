# Guide chords and their configuration menu

Status: partial. The two defects are fixed and the configuration page ships; Downloads is not an
action yet (`downloads-page.md`), and nothing has been tried on a real pad.

Requested: "Guide chord buttons with a configuration menu".

## The chord rule

One implementation, `gamepad/chord_gate:ChordGate`, used by the shell's controls
(`input/pad_chords:PadChords`, one gate per pad) and by each pad's virtual pad while a game holds
the pads (`gamepad/pad_translator:PadTranslator`, chords given by `Pads::setChords`).

- Any button may be a chord's modifier; the player chooses. A button is a modifier only while some
  bound chord starts with it, so unbound buttons cost nothing.
- A modifier's press is held back: neither the shell nor the game sees it yet.
- A bound partner pressed while the modifier is held plays the chord. The partner's press and
  release and the modifier's release are swallowed; the game sees neither button. The modifier
  stays a modifier while held, so it can play further chords.
- Any other button pressed while the modifier is held means no chord is coming: the modifier's
  press is let out first, then that button. From then on the modifier is an ordinary held button
  until it is released.
- A modifier released alone under the hold delay (`input/pad_chords:holdDelay`, 500 ms) is a tap: its
  press and its release come out together, the release in a report of its own. Held for the hold
  delay or longer and released alone, it does nothing. So Start bound as a modifier still pauses
  a game with a tap.
- Latency: a modifier reaches the game only when it is released (a tap) or let out by another
  button. A game that needs a held modifier alone (an analogue L2 as a brake) does not get it while
  that button starts a chord. An analogue trigger that is a modifier stays at rest on the virtual
  pad until it is let out, then reports its travel.
- The left stick's directions are edges like the d-pad's, so they play chords, but the stick axis
  itself still reaches the game.
- Guide never reaches the game; a Guide tap opens the Guide menu, a long Guide hold does nothing.

## Causes and fixes

- Chords leaked into the game: `gamepad/pads.cpp:Pads::run` wrote every translated event to the
  virtual pad, and `PadTranslator::translate` withheld only `BTN_MODE`. Fixed in
  `PadTranslator::translate`, which now builds the virtual pad from the physical state through the
  gate (`gate`, `sync`, `tap`).
- L2 and R2 chords never fired on analogue triggers: `PadTranslator::translate` made L2/R2 controls
  only from `BTN_TL2/BTN_TR2` on pads without trigger axes. Analogue triggers now press at
  `triggerPressAt` (0.5 of travel) and release at `triggerReleaseAt` (0.3), in
  `pad_translator.hpp`. This also gives `opensu-retro` its L2/R2 (S032).

## Actions and the page

- Chord-capable actions (`takesPadChord`): Quick menu, Volume up, Volume down, Mute, Close game,
  Save state, Load state, Reset game, Devices, Home. Adding one (Downloads) is an `Action`, its row
  in `shortcuts.cpp:infos` and a case in `app/shortcut_router:ShortcutRouter::perform`, which the
  compiler asks for. Close game and the retro actions toast "no game is running" without a game;
  save, load and reset go through `RetroSession::perform`, which the quick menu shares.
- Shipped chords hold Guide, which no game receives: Guide + A quick menu, Guide + Up/Down/Left
  volume (L2 before; as a modifier it would have kept a held L2 from every game by default). The
  game actions ship with none.
- Settings › Guide chords (`app/chord_settings`), next to Controls, which has a "Guide chords" row
  that opens it: one row per chord-capable action with its chord or "None"; A listens through
  `ShortcutEditor` (the first button held, then another; B alone cancels on its release), and
  "Restore default chords" puts the chords back and keeps the keys.
- Storage is unchanged: `shortcuts.pads` in `settings.json`. A change is pushed to the pads at once.
- `POST /input guide+a` holds buttons together over the control channel.

## Evidence

- Fail first: `pad_translator_test` failed "Guide + A sends no A to the game" and `pads_test`
  (uinput) failed the same check before the gate; `pad_translator_test` failed "past half its
  travel it is L2" before the analogue triggers. All pass after.
- Unit tests: `chord_gate_test` (the rule, Start as a modifier), `pad_translator_test` (chords,
  taps, long holds, let-out, Start tap reaching the game, analogue hysteresis and modifiers),
  `pads_test` (Guide + A and L2 + X forward nothing; an L2 tap arrives), `shortcuts_test`,
  `shortcut_router_test` (game actions), `settings_controller_test` (the page, capture, refusal,
  restore), `control_channel_test` and `control_bridge_test` (`guide+a`).
- Hidden shell (`scratch/guide-chords/verify.py`): `guide+a` opens the quick menu, a Guide tap
  opens the Guide menu, `l2+up` raises the volume display, the page is reached at
  "Settings > Guide chords", Devices captured as Guide + B and saved, then `guide+b` opens Devices.
- Parity scene `settings-guide-chords` in `tools/screenshots`; the quick menu scene now uses
  `guide+a`.

## Left

- Downloads as an action, with the Downloads page.
- On real hardware: an Xbox or DualSense trigger as L2, Guide + A and L2 + Up in a game sending
  nothing, a Start tap pausing a game, and a polling game seeing a replayed tap.
- The chord list shown on a long hold (`chord-hold-overlay.md`) reads its timing from the same gate
  and `holdDelay`.
