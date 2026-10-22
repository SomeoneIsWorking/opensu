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

Search wraps the keyboard in `ui/search_panel`: a card with the field and up to five results
stands above the docked keyboard, ending under the last row listed (`SearchLayout::cardFor`), and
the keyboard leaves out its own preview line (`layoutKeyboard(..., preview = false)`).
`app/search_controller` handles the results zone and passes every other button to
`actOnKeyboard`. While any keyboard is docked (`ui::Shell::dockedKeyboard`) the shell draws neither its bottom
nav dock nor the HUD's corner hints, toasts stand above the keyboard, and the keyboard shows its
own hints. The folder chooser stands in the screen above where its path keyboard docks
(`SettingsPanels::folderFrame`), so typing a path covers none of it. `ui/settings_panels` owns the path
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
- Pointer: `KeyboardLayout::keys` is one rectangle per grid key, or on the wheel the eight petals
  then the diamond's four faces (`daisyPetals + face`); hovering focuses it (`OnKeyboardKey`), a
  click presses A on it. Resting on a petal picks it until the stick picks one or the keyboard
  closes, so the pointer can move to the diamond and click a character; a click on a petal types
  nothing.

## Docking

Either layout docks along the bottom edge, centred, on a translucent card no taller than a fifth
of the frame: 92 dp plus a 4 dp margin is a fifth of the 480 dp reference height. The keyboard
draws in its own unit, `KeyboardLayout::unit`, the frame's dp shrunk when the dock would be taller
than a fifth or wider than the frame. Nothing above it moves and nothing under it is dimmed. Where
the caller shows no field of its own (the path, the password, later a game), a one-line preview of
the text sits above the strip: the prompt when empty, dots for a password, the end of a long line
kept in view with the caret.

## Daisy wheel (default)

Eight petals clockwise from the top, four characters each at the face positions X left, Y top,
B right, A bottom. Sets: letters, capitals, numbers, symbols (`daisyCharacter`); English only.

Drawn left to right: the ring of petals (each a dot with its four characters in miniature, the
picked one filled in ink, L3 in the middle), the set strip (LT, abc ABC 123 #+=, RT, LB Delete,
RB Space) over the hints, and the picked petal's four characters large in the A/B/X/Y diamond.
With no petal picked the diamond shows the face buttons themselves. The ring sits under the left
thumb's stick and the diamond under the right thumb's buttons.

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

Docked as a phone-style bottom keyboard, its hints in two lines beside the keys. Ten columns, five rows (digits, letters, `' - . & /`, Space, Delete, Clear, Done). D-pad walks,
A types, B deletes and leaves when empty, Y space, Select clear, Start done; X is Caps on a
password and "Results" in search; Up from the top row enters the results.

## Drawing

Discs, rings, keys and cards are gpu2d's anti-aliased SDF shapes (`fillCircle`, `fillRoundRect`,
`strokeRoundRect`, `ring`), not `ui/round_shape`'s tessellated meshes, which have no anti-aliasing.
A single character is centred on its ink (`Typeface::drawInkCentred`, from gpu2d's
`Text::ink` glyph bounds), not its advance box and baseline; words are centred on their advance.

## Invariants

- A search line never starts with a space or doubles one; paths and passwords take any.
- Lengths: search 64 bytes, path 4096, password 256.
- A password line is reserved to its full length on open and wiped (`host::wipe`) on close and on
  clear; it is drawn as dots, caret included.
- Typing inserts at the cursor; backspace deletes the character before it (UTF-8 aware).

## Checking it in the product

Run `opensu --hidden` and drive its control channel: `POST /input` with `search` opens search,
`up stick 3000ms` holds the stick up, `a` types, `r2` steps the set, `GET /frame.png` captures.
`up+a stick` presses the stick and A together in one batch. `OPENSU_WIDTH` and `OPENSU_HEIGHT`
set the window size; the dock's top should sit at or below four fifths of the frame's height.
