# Guide opens the macOS Games app

Status: open. Seen on the Mac (M2, macOS 26.7.3) on 2026-11-20; not fixed.

## Observed

- The player's Z01 controller shows up as a DualShock 4: openSU's log says "controller ready: PS4
  Controller", and SDL reads it through its HIDAPI driver.
- Pressing its Guide (PS) button opens openSU's Guide menu and also brings up the Games overlay (Apple's Games app panel).
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

## Tested (2026-11-20)

A small front app (an AppKit window plus GameController) found the pad as "DUALSHOCK 4 Wireless
Controller". Its Home and Options buttons report `isBoundToSystemGesture` true. The app set
`preferredSystemGestureState = .disabled` on them, and reading it back gave disabled. It ran
once with `shouldMonitorBackgroundEvents` off and once on (openSU's SDL sets it on through
`SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS`).

Both times, a Home press still brought up the Games overlay (Display, Sound and the controller's
profile) over the front app. The app's handler for the Home button never fired, while Options and
Menu presses reached it. So macOS takes the press before any app sees it, and the documented
opt-out doesn't stop it for this pad. SDL making the same request (below) can't fix it either.

## Not yet known

- Whether some other declaration stops it: an Info.plist game declaration, Game Mode support, or
  a signed app from the App Store.
- Whether the overlay can be turned off for the button in System Settings › Game Controllers, or
  through the controller's profile ("Default" in the overlay).

## Next

Look for a setting that turns the Home button's overlay off, in System Settings › Game Controllers
and in the overlay's controller profile. If there's one, that's the answer for players and goes
in openSU's Mac notes. openSU's pad code can't stop it from inside the app.
