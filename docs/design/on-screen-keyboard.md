# On-screen keyboard

One keyboard types into every openSU field: Global Search, a folder path (Settings › folder
chooser › Type a path) and the administrator password (Install session mode). Typing into games
and the browser is not built yet (`docs/issues/on-screen-keyboard.md`).

## Owners

| Part | Owner |
| --- | --- |
| The line, its cursor, the style, the grid's keys and walk, the wheel's petals and sets, layout and hit tests | `ui/on_screen_keyboard` (`OnScreenKeyboard`, `layoutKeyboard`) |
| Pad buttons, both styles | `app/keyboard_pad` (`actOnKeyboard`) |
| Drawing | `ui/on_screen_keyboard_painter` |
| The style chosen | `input/keyboard_style`, saved as `keyboard` (`daisy`, `regular`) in `settings.json`, Settings › Controls › Keyboard |
| Left stick to the wheel | `app/shell_app` (`wheelTakingStick`) |

Search wraps the keyboard in `ui/search_panel` (the results list sits in room the keyboard
reserves under its field, `layoutKeyboard(..., reserved)`); `app/search_controller` handles the
results zone and passes every other button to `actOnKeyboard`. `ui/settings_panels` owns the path
keyboard, `ui/session_panels` the password keyboard. `ui::Shell::setKeyboardStyle` hands the
setting to all three; an open keyboard keeps the style it opened in.

## Inputs and outputs

- Pad buttons from `ShellApp::actOn` through the owning controller to `actOnKeyboard`, which says
  `Handled`, `Edited`, `Confirm` or `Leave`.
- Left stick directions: pad sources mark them (`gamepad::Event::stick`, set by `PadTranslator` and
  `SdlPadState`). While a daisy wheel takes input, `ShellApp::handleEvents` sends their presses and
  releases to `OnScreenKeyboard::stick` instead of the direction repeat; otherwise the stick moves
  focus as the d-pad does.
- Physical keyboard text goes straight into the line (`type`, `backspace`), either style.
- Pointer: `KeyboardLayout::keys` is one rectangle per grid key or per wheel character
  (`petal * 4 + face`); hovering focuses it (`OnKeyboardKey`), a click presses A on it.

## Daisy wheel (default)

Eight petals clockwise from the top, four characters each at the face positions X left, Y top,
B right, A bottom. Sets: letters, capitals, numbers, symbols (`daisyCharacter`); English only.

| Button | With a petal picked | With the stick centred |
| --- | --- | --- |
| Left stick | picks the petal it points at (diagonals are the petals between) | |
| A, B, X, Y | types that face's character | A: nothing, B: leave, X: delete, Y: space |
| LB / RB | delete / space | delete / space |
| LT / RT | previous / next set | previous / next set |
| D-pad Left / Right | moves the cursor | moves the cursor |
| D-pad Down (search) | the results | the results |
| Select / Start | clear / done | clear / done |

The right stick is not read for the cursor; the shell has no right stick events.

## Regular grid

Ten columns, five rows (digits, letters, `' - . & /`, Space, Delete, Clear, Done). D-pad walks,
A types, B deletes and leaves when empty, Y space, Select clear, Start done; X is Caps on a
password and "Results" in search; Up from the top row enters the results.

## Invariants

- A search line never starts with a space or doubles one; paths and passwords take any.
- Lengths: search 64 bytes, path 4096, password 256.
- A password line is reserved to its full length on open and wiped (`host::wipe`) on close and on
  clear; it is drawn as dots, caret included.
- Typing inserts at the cursor; backspace deletes the character before it (UTF-8 aware).

## Checking it in the product

Run `opensu --hidden` and drive its control channel: `POST /input` with `search` opens search,
`up stick 3000ms` holds the stick up, `a` types, `r2` steps the set, `GET /frame.png` captures.
`up+a stick` presses the stick and A together in one batch.
