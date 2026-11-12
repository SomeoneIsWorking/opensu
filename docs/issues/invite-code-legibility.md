# Invite code legibility: Z and 2, 9 and Y look alike

Status: open. Found 2026-11-12 reading an Android host's code off its frame.

Observed: Settings › Play online shows the invite and reply codes in the interface theme's body
font (`app::OnlineScreen`, `tk::TextRole::Body`, Cal Sans in the default theme). In it `Z` and `2`
differ only in a few pixels of their top bar, and `9` and `Y` are close enough that tesseract
read the group `A9BS` as `AYBS` at every scale it tried. It also read `6Z57` as `6257` at three of
four scales. A friend reading the code aloud, off a TV across the room or off a photo of the screen
makes the same mistakes.

Expected: every symbol of a code is unmistakable at the sizes the shell draws it, on a phone and on
a TV.

Cause: the font. The code is Crockford base32 after `OSU0` (`lucent::p2p` descriptor,
`kAlphabet`). The decoder already folds the confusable letters Crockford leaves out (`O` to `0`,
`I` and `L` to `1`), so those cost nothing. `Z` and `2` are both symbols of the alphabet, and the
body font draws them alike, as it does `9` and `Y`; `S`/`5`, `B`/`8` and `G`/`6` are close too.

A wrong symbol is never taken for another code: the code carries a 2-byte check, so a misread is
refused as "A letter is wrong or missing" (`CodeProblem::Damaged`) and leaves the invite unused. The
cost is a friend who cannot get in and does not know which letter is wrong.

## Fix options

1. Draw codes in a face whose `Z`, `2`, `9`, `Y`, `S`, `5`, `B`, `8`, `G`, `6` and `0` are distinct (a
   monospaced code face with a crossed `Z` and a slashed or dotted `0`), as a text role of the
   toolkit that every code display uses. No wire change; it helps reading aloud, off a TV and off
   a photo. Needs a font with a licence we can ship.
2. Accept both readings on the way in: a decoder that, for a damaged code, tries `Z`/`2` (and the
   other close pairs) at each position and takes the one variant the check accepts. No wire change,
   but the check is 2 bytes, so each extra variant tried raises the odds of a wrong code passing
   (1 in 65 536 per variant); it is only safe for a few swaps, and it hides which letter was wrong.
3. Drop `Z` (and any other symbol the chosen face still draws close to a digit) from the alphabet. A
   wire change: old and new openSUs could not read each other's codes, so it needs a new code
   version (`CodeProblem::Version` covers the refusal).

## Proposed

Option 1, as reviewed for the UI: a `TextRole::Code` drawn in a monospaced face made to tell
characters apart (Atkinson Hyperlegible Mono Bold or B612 Mono Bold, both OFL 1.1, to be checked
rendered for `Z`/`2` before choosing), about 1.2 times Body, slightly tracked, a fixed four groups a
line so a reader can say "line 3, group 2", and the same face for the text typed into the reply
field. Themes may size it but not swap its face. The toolkit's typesetter and interface text hold
one face today, so a second face is the main work. A QR code beside the code would serve the photo
case better than any face; the code is about 72 symbols long. Option 2 at most for one or two swaps;
option 3 only if the face still leaves a pair alike.
