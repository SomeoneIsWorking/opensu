# A Steam Input game in Isolated Steam has no pad

Seen 2026-11-19, Slay the Spire (646570) played from the Mac over remote play, host in Isolated
mode: picture and sound, but no button reached the game.

## What happens

- The guest's presses reach the host's pad: `/dev/input/event23`, one of the four kept virtual
  pads (`steam::isolatedPadSlots`, Xbox 360 ids `045e:028e`), carried hat-down, `BTN_SOUTH` and
  `BTN_EAST` at the times the guest sent them.
- Steam inside the sandbox holds all four kept pads open for Steam Input, and starts the game with
  `045e/028e` in `SDL_GAMECONTROLLER_IGNORE_DEVICES`, so the game's SDL ignores them and waits for
  Steam's own virtual pad. The game holds no `/dev/input` node.
- Steam makes that pad through `/dev/uinput`, which the sandbox does not have. Binding it would not
  be enough: the sandbox's `/dev/input` holds only the nodes bound when Steam started, so a pad
  Steam creates later never appears inside.

## Decision

Turn Steam Input off for the games openSU starts in Isolated mode, so they read openSU's virtual
pads directly, as the games without Steam Input already do. `/dev/uinput` in the sandbox was
rejected: it would also let anything inside create a keyboard that types into the desktop session,
and it still would not show Steam's pad to the game.

## Next

Steam's library sets the per-game override with
`SteamClient.Apps.SetThirdPartyControllerConfiguration(appid, value)`, kept in the app's details as
`eEnableThirdPartyControllerConfiguration` (three values: Steam Input off, default settings, on;
the numbers are not read yet). Set it off through `steam::devtools` before a launch in Isolated
mode, and show Slay the Spire's menu moving from the Mac's pad.
