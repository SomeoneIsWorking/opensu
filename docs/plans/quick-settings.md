# Quick settings

A panel down the right edge of the screen with the settings a player changes mid-game, like the
Steam Deck's quick access menu or the ROG Ally's command center. The player picks which settings it
shows and in what order.

## What exists

- The Guide menu (`app::GuideMenuController`) has a card beside its places with Volume, Mute,
  Brightness, Power profile, Interface size, then Screen and Profile, and the page's shelf rows.
  Volume, brightness and the power profile are set there and nowhere else; Settings › Audio only
  says "Volume: in the Guide menu", and Devices › Display says the same of brightness.
- The Quick menu action (Guide + A, Ctrl+Tab; `input::Action::QuickMenu`, done in
  `ShortcutRouter`) opens that same Guide menu focused on Volume.
- Settings › Appearance has Night light and Interface size (`interface_size_setting`); Devices ›
  Controllers has the stick lights (`RingLightSettings`) and Devices › Bluetooth the adapter.
- No Wi-Fi backend and no performance overlay exist, so neither is offered.

## Design

- **One owner, `app::QuickSettings`.** It owns the settings a quick panel can hold (Volume, Mute,
  Brightness, Power profile, Interface size, Night light, Stick lights), the row each is and what
  its row does, through the owner of the thing (`VolumeControl`, `BrightnessControl`,
  `host::PowerProfiles`, `Preferences`, `RingLightSettings`). A setting the machine lacks (no
  backlight, no PowerStation, no stick lights, no night light) is not offered.
- **The player's choice** is `settings::Settings::quickSettings`, the spellings in order; unset
  means the default set (Volume, Mute, Brightness, Power profile, Interface size), so a new default
  reaches a player who never chose.
- **The same rows everywhere.** Settings › Audio shows Volume and Mute, Settings › Appearance
  Brightness, Night light and Interface size, Settings › Power the power profile (only with
  PowerStation), each `QuickSettings`' row with its id, and Settings hands those ids back to it.
  The stick lights stay on Devices › Controllers, from `RingLightSettings` as now.
- **Settings › Quick settings** arranges them: one row per offered setting, the shown ones first in
  their order; each is a choice of its place ("1", "2", …) or "Not shown", picked from the list A
  (or a tap) opens. Restore defaults puts the default set back. A setting this machine does not
  offer keeps its spot in the saved order, for another machine.
- **The panel (`app::QuickSettingsPanel`)** is an overlay on the toolkit stack at the right edge
  (the whole width on a narrow screen): the heading, the chosen rows on a card, scrolling when tall,
  and "Choose what shows" leading to Settings › Quick settings. Back, Start or a tap beside it
  closes it; the same shortcut closes it again. Over a game it is interactive like any menu, and the
  game is not held, so a volume change is heard as it is made.
- **The shortcut.** The Quick menu action becomes Quick settings (same spelling `quickMenu`, so
  saved remaps keep) and opens the panel instead of the Guide on Volume. Guide closes the panel and
  opens the Guide menu.
- **The Guide menu's card** shows the same chosen rows from `QuickSettings`, then its own Screen and
  Profile and the shelf rows; it no longer sets volume, brightness or the power profile itself.
- The volume display stays hidden while the panel is open, its Volume row being the display.

## Proof

Hidden run with its own config (done): Guide + A and Ctrl+Tab open the panel with the defaults and
close it again, B and a tap beside it close it; a pick on Settings › Quick settings moves or hides a
setting and the panel follows; Interface size grows from the panel and the screen follows; the
Guide menu's card shows the same rows. Not yet shown: the panel over a running game, and the
brightness, power profile and stick lights rows on a handheld. Focused tests: `quick_settings_test`, `guide_menu_controller_test`,
`settings_controller_test`, `settings_test`, `shortcuts_test`.

## Later

- Bluetooth's switch, once `BluetoothSession` polls without the Devices page open.
- Wi-Fi and a performance overlay, once either has a backend.
