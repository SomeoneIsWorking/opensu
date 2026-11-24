# The guest's mouse in a remote game, where the player points, and locked when the game locks it

Requested 2026-11-24 from the Mac: "We didn't add any mouse lock so playing KB&M is difficult for
games that have mouse lock" and "Stream mouse position is not reflected correctly".

## What is wrong today

Reproduced on fedora's host (Gamescope 1920x1080) with a hidden 1440x900 guest:

- `stream/host_input` moves the pointer with XTest on Gamescope's Xwayland (`session/xtest_input`),
  the guest's fraction of the picture taken across the whole X screen. Gamescope's frame is the
  guest's shape with the game's window fitted in, bars included, so the fraction lands off by the
  bars: in Slay the Spire, the picture's top edge (frame y 45 of 900) went to screen y 54, its
  bottom (855) to 1026 instead of 1079.
- A game larger than the screen (Slay the Spire picks 3840x2160 on the 1920x1080 screen) cannot be
  reached past the screen's size: XTest points are screen pixels, the game's are window pixels.
- A game that locks the mouse sets an empty cursor and warps the pointer back after every move
  (Slay the Spire, from a few seconds after its menu shows): each absolute move reads to it as a
  large jump, and the pointer never stays where the guest put it.
- A built-in runner's picture is streamed at its own size and shown at its shape (`Shape`), while
  the runner's window letterboxes it on the host: x lands squeezed toward the edges.

## What it does

- The host sends the guest's pointer to Gamescope through its input-emulation socket
  (`LIBEI_SOCKET`, Gamescope's `InputEmulation`) with libei: absolute positions in the focused
  window's own pixels (`wlserver_mousewarp`), relative motion (`wlserver_mousemotion`, which keeps
  a game's pointer lock), buttons and the wheel. Gamescope takes them as its own pointer, so its
  cursor, its fit and its pointer constraints all hold. Keys and typing stay on XTest.
- The fraction the guest sends is taken through Gamescope's fit (`calc_scale_factor_scaler` and the
  centring in `compute_window_transform`) from the frame to the window, so the bars fall outside.
- While the game's cursor is empty (XFixes cursor image with no opaque pixel), the host tells the
  guest the pointer is locked. The guest puts its window in relative mouse mode and sends motion
  (`MouseState::moveX`/`moveY`) instead of positions; the Guide or any screen over the game lets go.
- A built-in runner's pointer is taken over its picture as the runner draws it.

## Order

1. Done: a guest never withdraws its only codec (`GuestStream::withdraw`), which ended the session
   whenever a guest without hardware decoding fell behind.
2. The build image and packages carry libei; `session/ei_pointer` connects to `LIBEI_SOCKET` and
   sends absolute, relative, buttons and wheel; `stream/host_input` uses it for the pointer.
3. `MouseState` carries the picture size the guest pointed over; the fit mapping is one function
   with a test against Gamescope's numbers (1440x900 frame, 3840x2160 window: y 45 maps to 0).
4. `PacketKind::Pointer` from the host when the cursor's emptiness changes and with each facts
   tick; the guest's relative mode in `ShellApp` and `RemoteInput`.
5. Proof on fedora's host with the hidden guest: Slay the Spire's Play button clicked from the
   guest at its place in the picture; its own cursor follows relative motion once it locks.
