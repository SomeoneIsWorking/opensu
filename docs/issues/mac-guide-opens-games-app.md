# Guide opens the macOS Games app

Status: open. Seen on the Mac (M2, macOS 26.7.3) on 2026-11-20; not fixed.

## Observed

- The player's Z01 controller shows up as a DualShock 4: openSU's log says "controller ready: PS4
  Controller", and SDL reads it through its HIDAPI driver.
- Pressing its Guide (PS) button opens openSU's Guide menu and also opens Apple's Games app.
- It happens with openSU on its home screen and no game running, and with a Wine game shown
  inside openSU's window, so it isn't caused by the embed work (`docs/plans/mac-wine-embed.md`).
- The player says Guide opens the Games app "always regardless of openSU".

## What is known

- macOS 26 hands a controller's Home button to the Games app unless the front app asks it not to,
  through the GameController framework: `preferredSystemGestureState = .disabled` on the button.
- SDL (3.4.18, `src/joystick/apple/SDL_mfijoystick.m`) does that for every button bound to a
  system gesture, as each `GCController` connects (`IOS_AddMFIJoystickDevice`), before it lets
  HIDAPI take a DualShock 4. It sets it back to enabled only when a controller it opened through
  GameController closes (`IOS_JoystickClose`), which a HIDAPI pad never is. So SDL's request
  should be in force for this pad, and the Games app opens anyway.

## Not yet known

- Whether `isBoundToSystemGesture` is true for the Home button when SDL asks, on macOS 26; SDL
  skips the button when it is false.
- Whether macOS honours the request from an app with no Info.plist game declarations, or only
  from the front app's own `GCController` objects at the moment of the press.

## Next

A small test program that, in front, sets `.disabled` on every controller's `buttonHome` whether or
not it reports a system gesture, and logs both values when the button is pressed. Then the fix goes
in openSU's pad code (macOS only), or the player switches the button off in System Settings ›
Game Controllers if macOS gives apps no say.
