# Quick settings

A panel down the right edge of the screen with the settings a player changes mid-game. The player
decides what is in it, not openSU.

## The player's design

In the player's words:

- "either all the settings should be in "Settings" or we need a right panel quick menu"
- "who decides what goes in quick menu and what is out? I don't want to add a "add to quick menu"
  to every option either" — then: "we can add a customize button in quick settings where user can
  customize quick settings tabs (just icons), categories and items"
- "the defaults would be volume, brightness, power and..."
- What was agreed when this was filed (#54, 2026-11-24): default tabs "System: volume, mute,
  brightness, power profile, Bluetooth, night light. Game: that game's settings. This replaces the
  side panel next to the Guide. View: the page's sort and filters. Device: pad battery and the
  Ally's lights and sticks. Every setting also lives in Settings, and the Guide keeps only places
  and actions."
- "I wanted the user to be able to add anything from the settings into quick menu"
- "reorder should be more intuitive like in edit mode, you can grab an item and move it up and
  down"
- "you also didn't remove the quick settings part in the guide menu"
- "you can add pin there but make sure my design stays like quick settings will have an edit option
  and icon tabs"

So: icon-only tabs; each tab holds categories; each category holds items, and an item is any
Settings row; an edit mode (Customize) arranges all three by grabbing and moving; Settings rows can
also be pinned from Settings itself; the Guide menu has no quick rows.

## Design

- **Layout, `settings::QuickLayout`** in `settings::Settings::quickSettings`: tabs in order, each an
  icon name and its categories; a category is a name and its items; an item (`QuickPin`) is a
  Settings page id and a row id there. Unset means the default layout, so a new default reaches a
  player who never customized. The default tabs are the ones agreed above: System (volume, mute,
  brightness, power profile, Bluetooth once it has a switch, night light), Game (the running
  game's settings, replacing the Game settings panel beside the Guide), View (the page's sort and
  filters) and Device (pad battery, the Ally's stick lights and sticks).
- **`app::QuickLayoutEditor`** owns every change to the layout (add, remove, move, rename, change
  icon, pin, unpin) and saves through `Preferences`. Nothing else writes `quickSettings`.
- **An item is a live Settings row.** The panel looks each pin up through
  `SettingsController::page(page)` and draws that row with `ui::addRow`; A, Left/Right and sliders
  go back to `SettingsController::run`/`setLevel`, so a quick item behaves exactly as it does in
  Settings. A pin whose row this machine does not show (no backlight, no PowerStation) is skipped
  and kept, for another machine. Rows that ask a question first, pages, folders, facts and
  headings cannot be pinned.
- **`app::SystemControls`** owns the machine's own controls (volume, mute, brightness, power
  profile, interface size, night light, the screen's turn, stick lights) as Settings rows on
  Audio, Appearance, Power and Controls, the only place they are set. Playing as is a choice at
  the top of Settings › Profiles, and Hidden games only a row on Settings › Library, so every row
  the Guide's card had is a Settings row the panel can hold.
- **The panel (`app::QuickSettingsPanel`, `QuickSettingsScreen`)**: an overlay at the right edge
  (the whole width on a narrow screen). At the top, the tabs as icons only; LB/RB step them, a tap
  picks one. Under them the chosen tab's categories, each a heading over its items, scrolling when
  tall; at the end, Customize. Back, Start or a tap beside it closes it; the Quick settings shortcut
  (Guide + A, Ctrl+Tab) opens and closes it. The game is not held, so a change is heard and seen as
  it is made.
- **Customize (edit mode)**, from the Customize row or View/Options on the panel:
  - every tab, category and item is a plain row; A grabs it and A again drops it; while grabbed,
    Up/Down move an item (across categories at their edges) or a category, and LB/RB move a tab;
    B puts a grabbed thing back where it was, else leaves edit mode; a tap on another place drops
    a grabbed thing there;
  - X removes the focused item, category (with its items) or tab (with its categories);
  - Y on a category renames it on an entry card;
  - Y on a tab changes its icon, from the icon list;
  - each category ends with "Add a setting", which browses Settings' categories and pages and adds
    the picked row to that category; the tab ends with "Add a category"; the tab strip ends with a
    "+" tab; Done leaves edit mode.
- **Pinning from Settings**: View/Options on a pinnable Settings row adds it to the first tab's last
  category, or takes it out of Quick settings when it is in; the hint names which, and a toast
  says it was done.
- **Icons** are openSU's own drawings under `assets/icons/quick/`, drawn as tinted masks.
- **The Guide menu** keeps only places and actions: no quick rows, no Game settings panel beside
  it.
- The volume display stays hidden while the panel is open, its Volume item being the display.

## Built and not yet built

Built: the layout and its editor, the System, Game, View and Device tabs, the panel with icon tabs
and Customize, Add a setting, the icon list, View-to-pin in Settings, and the Guide without its
card.

The Game tab (`QuickTab::game`) lists the running game's setting categories through the layered
editor's own `EditorPages`, each opening that category's editor page as a panel beside it; Game
settings in the Guide closes the menu and opens Quick settings there. It is always in the layout
(second in one kept without it), moves and changes its icon, but is not removed and holds no
categories of its own. Single game settings are not pinned into other tabs: a pin is a Settings row,
and the game's rows are the editor's.

Not yet built, next in order:

1. **Pad battery** on the Device tab: a fact row from Devices' controller roster, which is not a
   Settings page either.
2. **Bluetooth** on System, once `BluetoothSession` polls without the Devices page open.

## Proof

Hidden run with its own config: the panel opens with the default tabs and categories; Customize
grabs Volume and moves it under Brightness; adds a tab with another icon and a category on it,
renames the category, adds Night light from Settings › Appearance through "Add a setting"; Options
on Settings › Audio › Mute takes it out; the Guide menu shows places and actions only. Focused tests:
`quick_layout_test`, `system_controls_test`, `guide_menu_controller_test`,
`settings_controller_test`, `settings_test`, `shortcuts_test`.

## Later

- Wi-Fi and a performance overlay, once either has a backend.
