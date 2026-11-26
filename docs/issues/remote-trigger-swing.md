# Remote Spider-Man: RT does not swing

Status: open, unverified since the seat fix.

Reported by the player (2026-11-25, streaming from fedora to the Ally): "I tried Spider-Man then and
controller worked but I think it was mapped as keyboard because A didn't jump for example, only
some buttons were working, I couldn't jump or swing"; after the first fix: "Now jump works but I
can't swing using RT, I checked if RT is broken but openSU -> Devices shows it being pressed"; and
"it feels like RS right is stronger than RS left (camera rotate)".

## Known

- Jump (A) was fixed by `19f6fa52` (the guest sits at the first free pad, which a one-player game
  reads). The stick asymmetry is what calibration (`c1f05a88`, `4afa9218`) and `2ad27d75` (the host
  reads the sticks hall-corrected) address.
- The swing test that compared the trigger reaching 208 against 255 on the host was inconclusive:
  the player was playing at the same time and Spider-Man stayed on the ground.

## Next

Record fedora's virtual pad (`evtest` on the guest seat's event node) while the Ally holds RT fully
and turns RS both ways: RZ must reach 255 and RX both ends. Then a swing in Spider-Man, seen on
`/remote/frame.png`.
