# Touch layouts by console and genre

Requested 2026-11-17, relayed from the user by the macOS session: "Today every non-arcade system
shows only the usual pad (a D-pad plus buttons), which can't play most games. Layouts should match
what the game needs, picked automatically, with a per-game override in Game settings (same layers
as Touch controls size)": per console layouts, a full modern gamepad for remote play, genre presets
on top (racing, platformer, and the keyboard-and-mouse shooter of [ideas](../ideas.md)), under the
same rules as now.

## What it does

- A Touch layout setting at every layer, beside Touch controls and their size: Auto (default), or
  one of the layouts below. Auto picks by the game's console; a game on another openSU, a store
  game or one of your own, gets the gamepad.
- The layouts, each in the console's own letters and places:
  - **Two buttons** (NES, Game Boy, Game Boy Color): d-pad, B and A, Select and Start.
  - **Game Boy Advance**: the same with L and R.
  - **Super Nintendo** (and DS): d-pad, Y X B A in a diamond, L and R, Select and Start.
  - **Nintendo 64**: stick, d-pad, A and B, the four C buttons, Z, L and R, Start.
  - **PlayStation** (PS1, PS2, PSP): d-pad, the four symbols, L1 L2 R1 R2, Select and Start, and
    both sticks with L3 and R3 when the game reads them (the core names analog controls).
  - **Genesis 3-button** and **Genesis 6-button**: d-pad, A B C (and X Y Z, Mode), Start. openSU
    has no Genesis system yet, so Auto never picks them.
  - **Gamepad** (GameCube, Wii U, Switch, Xbox, Xbox 360, PS3, PS4, Dreamcast, remote play): two
    sticks, d-pad, A B X Y, bumpers, triggers, Back and Start.
  - **Pad**: today's d-pad, A B X Y, L and R, Select and Start, for a console with none of its own.
- Genre presets, picked in the same setting: **Racing** (steering on the left; gas and brake pedals
  on the right, the triggers on the gamepad and the bottom and left face buttons on a console, plus
  the face buttons a race uses) and **Platformer** (d-pad, or the stick on an analog console, and
  big Jump and Action buttons, the bottom and left faces).
- A stick on screen is analog: how far the finger is from its centre is how far the stick leans,
  reaching the core as the pad's stick and the host as the virtual pad's.
- Same rules as now: beside the picture where it fits, under it upright, never smaller than a
  thumb, sized by Touch controls size, hidden by Dynamic. An arcade game keeps its machine's panel.

## Owners

- `settings/video_settings`: `touch.layout`, its choices, and Auto's rule by system
  (`resolveTouchLayout`), carried to the runner in `VideoSettings` as the other video settings are.
- `touch/console_layout` (replacing `touch/pad_layout`): one builder of every layout from its
  description (left cluster, right cluster, shoulder rows, middle pills, sticks).
- `touch/touch_controls`: an analog `Stick` (left or right) beside the 8, 4 and 2-way ones.
- `touch/touch_layer`: runs the layout it is given and reports the sticks held (`sticks`).
- `retro/runner_touch`: the layout from the video settings, the sticks to the joypad as the pad
  `touch`'s.
- `app/remote_touch`: the gamepad by default, its sticks into `RemoteInput`.

## Order

1. Analog touch sticks, the layout builder, the console layouts and the setting with Auto. Shown on
   a hidden runner per console (N64, PS1, SNES, GBA, NES) and on the tablet.
2. The gamepad for remote play, its sticks to the host. Shown with a hidden remote session.
3. Racing and Platformer.
4. The shooter: keys and swipe-to-aim through the host's virtual keyboard and mouse, for remote PC
   games ([ideas](../ideas.md)).
5. Genres told from what the store says (Steam's genres) to pick Racing, Platformer or Shooter on
   Auto.

## State

Steps 1 and 3 built: the setting with Auto, every console layout, Racing and Platformer, analog
sticks to the core, and the gamepad by default in remote play with its sticks sent to the host.
Shown in hidden runner frames per layout; remote play and the tablet not yet shown. Genesis layouts are reachable only
by hand until openSU has a Genesis system. Step 4 built: the Shooter's keys, mouse buttons and
swipe-to-aim as relative mouse motion through XTest; tested through the touch layer and the wire,
not yet shown in a remote session. Step 5 not started: openSU keeps no genres yet.
