# Touch controls that step aside for a controller

Requested 2026-11-07: "we can hide touch controls when using a controller but hotswap style,
dynamically, this also should be an option and dynamic by default".

## What it does

- While a game runs, the on-screen controls hide the moment a controller is used and come back the
  moment the screen is touched again, without a restart or a menu.
- A Touch controls setting, at every layer: Dynamic (default), Always, Never.

## Order

Steps 1 to 3 are built (`retro/runner_app` keeps an `input::LastDevice`, `runner_touch::touchWanted`,
`touch.show` in `settings/video_settings`); step 4 is open.

1. The runner knows which device gave the latest real input: `src/input/last_device` already
   answers that for the shell; the runner's own input (`retro/presenter` `WindowInput`) reports
   pad and finger events to the same rule.
2. `retro/runner_touch` shows or hides its layer from that, held fingers released when it hides.
3. The setting in the layered editor's touch section, read by the runner from its launch options.
4. Proof on the phone: a game with touch controls showing, a pad button press hides them, a tap
   brings them back.
