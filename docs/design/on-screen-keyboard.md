# On-screen keyboard

One keyboard types into every openSU text field (gpu2d `TextField`): Global Search, a folder
path (the folder chooser's Path field), the SteamGridDB key and the administrator password
(Install session mode). It is an input method: the text lives in the field, the keyboard holds
none. Typing into games
and the browser is not built yet (`docs/issues/on-screen-keyboard.md`).

## Owners

| Part | Owner |
| --- | --- |
| The style, capitals, the grid's keys and walk, the wheel's petals and sets, layout and hit tests | `ui/on_screen_keyboard` (`OnScreenKeyboard`, `layoutKeyboard`) |
| Opening, closing and the pad buttons, both styles | `app/keyboard_pad` (`KeyboardPad`) |
| Drawing | `ui/on_screen_keyboard_painter` |
| The style chosen | `input/keyboard_style`, saved as `keyboard` (`daisy`, `regular`) in `settings.json`, Settings › Controls › Keyboard |
| Docking it under the screens, pointer on its keys | `ui/shell` (`keyboard`, `aboveKeyboard`) |

A on a focused field asks the navigator for a keyboard (`Navigator::setKeyboard`, which
`ShellApp` points at `KeyboardPad::open`); search, the path, the key and the password open it
with their card; opening it starts editing the focused field (`Navigator::edit`), closing it
stops. `KeyboardPad::follow` closes it each frame once the field is not being edited (the focus
left it, or Submit; `Navigator::editing`). While it is open the shell draws neither its bottom nav dock nor the HUD's
corner hints, toasts stand above it, it shows its own hints, and overlay cards lay out above it
(`Navigator::layout`'s covered strip) so the field typed into stays in view.

## Inputs and outputs

- Pad buttons from `ShellApp::actOn` to `KeyboardPad::act` while it is open; it types through
  `Navigator::text`, deletes with `Action::Erase`, clears with Select (the field's Clear,
  `Action::Tertiary`, which is Y with the keyboard closed) and finishes with `Action::Submit`, so
  each field's own rules (length, no leading space) apply once. While it is open with a pad the
  screen's hint bar shows nothing (`InterfacePrompts::bound`); the keyboard's own row names the
  buttons.
- Left stick directions: pad sources mark them (`gamepad::Event::stick`, set by `PadTranslator` and
  `SdlPadState`). While the wheel is open (`KeyboardPad::takesStick`) `ShellApp::handleEvents`
  sends their presses and releases to `KeyboardPad::stick` instead of the direction repeat;
  otherwise the stick moves focus as the d-pad does.
- Physical keyboard: while a field is focused, text goes to it, Backspace is Erase, Enter is
  Submit, Escape is Back, Up and Down move, Left and Right move the caret once the field is being
  edited (typed into, or opened with the keyboard) and the focus otherwise (`ui::typingKeys`),
  either style and with the keyboard open or not; the hint bar then names only those (no "Type",
  no "Clear").
- Pointer: `KeyboardLayout::keys` is one rectangle per grid key, or on the wheel the eight petals
  then the diamond's four faces (`daisyPetals + face`), each a part of `ui::KeyboardChrome`, the
  keyboard's chrome screen; hovering focuses it, a click presses A on it, and the strip it covers
  takes every other click. Resting on a petal picks it until the stick picks one or the keyboard
  closes, so the pointer can move to the diamond and click a character; a click on a petal types
  nothing.

## Docking

Either layout docks along the bottom edge, centred, on a translucent card no taller than a fifth
of the frame: 92 dp plus a 4 dp margin is a fifth of the 480 dp reference height. The keyboard
draws in its own unit, `KeyboardLayout::unit`, the frame's dp shrunk when the dock would be taller
than a fifth or wider than the frame. Full screens under it do not move and nothing under it is dimmed;
the field it types into is always on a card above it.

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
| A, B, X, Y | types that face's character | A: nothing, B: closes the keyboard, X: delete, Y: space |
| LB / RB | delete / space | delete / space |
| LT / RT | previous / next set | previous / next set |
| D-pad Left / Right | moves the field's caret a character | moves the field's caret a character |
| Start | done | done |

The right stick is not read for the cursor; the shell has no right stick events.

## Regular grid

Docked as a phone-style bottom keyboard, its hints in two lines beside the keys. Ten columns, five
rows (digits, three of letters with `' - . &`, then `/`, Space, Delete, Done). D-pad walks, A types, B deletes and closes the
keyboard when the field is empty, X turns capitals on and off, Y types a space, Start is done.
The D-pad is the keys', so the caret moves only with a physical keyboard's arrows.

## Drawing

Discs, rings, keys and cards are gpu2d's anti-aliased SDF shapes (`fillCircle`, `fillRoundRect`,
`strokeRoundRect`, `ring`), not `ui/round_shape`'s tessellated meshes, which have no anti-aliasing.
A single character is centred on its ink (`InterfaceText::drawInkCentred`, from gpu2d's
`Text::ink` glyph bounds), not its advance box and baseline; words are centred on their advance.

## Invariants

- The keyboard holds no text; the field keeps its caret (gpu2d `TextField`) and the keyboard is up
  exactly while the field is being edited (`Navigator::edit`, `Navigator::editing`).
- Done closes the keyboard and submits the field; B closes only the keyboard, a second B is the
  card's Back.
- The field owns its rules: a search line never starts with a space or doubles one; lengths are
  search 64 bytes, path 4096, key and password 256; a masked field draws dots and wipes its copy.

## Checking it in the product

Run `opensu --hidden` and drive its control channel: `POST /input` with `search` opens search
with the keyboard up,
`up stick 3000ms` holds the stick up, `a` types, `r2` steps the set, `GET /frame.png` captures.
`up+a stick` presses the stick and A together in one batch. `OPENSU_WIDTH` and `OPENSU_HEIGHT`
set the window size; the dock's top should sit at or below four fifths of the frame's height.
