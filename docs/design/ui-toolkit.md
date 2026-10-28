# UI toolkit and settings schema

Status: steps 1 to 4 built (see 5): every screen, Home included, is on the navigator and the pointer reaches the shell only through `Navigator::pointer`. `gpu2d::ui` (gpu2d `docs/ui-toolkit.md`) has the theme, layout, widget
tree, focus, `Navigator`, the widgets and motion; openSU's Settings screen runs on it
(`src/app/settings_screen.*`, `src/ui/screen_stack.*`, `src/ui/interface_*`). Step 2: every
layered setting is declared once in `settings::Schema` (`src/settings/setting_schema.*`: video,
shader preset, frame generation, the emulator pick, each core's options), and one editor
(`src/app/layered_editor.*` over `src/app/layered_settings.*`) edits any scope from Settings ›
Emulators, a game's details and the Guide menu (a panel over the game, changes applied live);
`game_settings_page`, `shader_preset_picker`, `settings_tabs` and their painters and hit tests are
deleted. Not yet: core options' `SET_CORE_OPTIONS_DISPLAY` is not honoured, so options a
core hides for the loaded game (mGBA's Game Boy ones on a GBA game) are listed.

The player: menus "aren't intuitive", it "looks rushed", "layered
config not implemented well. Same for frame generation", and "a UI library like code is in order".

Evidence: every screen captured at 1280x800 with a map of how each is reached
(`scratch/ux-review/`, gitignored), and a survey of `src/ui`, `src/app`, `src/settings` at 708bd96.

## What is wrong

### Behaviour the player sees

- Five ways to change a choice: Settings rows (A opens the row, then Left/Right), game settings
  tabs (tick Override, then A opens a list where Left/Right do nothing), layer pages (Left/Right at
  once, the first step only pins "Core" with no visible change), the START panel (Left/Right at
  once, A steps), the details emulator button (Left/Right cycle, A steps).
- Layered settings: an Override checkbox beside the value is two controls for one decision;
  unticking throws the chosen value away; "Default" never says which layer a value comes from; the
  Global tab offers "Not set, the layer below decides" with nothing below it; in game the layer
  tabs shrink to "…"; the panel is titled "Game settings" on the Global tab; picking an emulator
  counts as "1 custom setting" while no row shows an override.
- Settings › Emulators is one flat list of about 80 rows with no headings.
- A core option changed in game is not kept: nothing is written and it is back after relaunch.
- Frame generation: rows exist only inside Gamescope, so outside it a Steam game's details lose
  Game settings; focus opens on a heading that cannot be selected; Fixed and Adaptive add rows with
  no explanation ("Display refresh", Tolerance "None", Flow scale); a ROM reads "Lossless Scaling,
  from Steam"; with Global Off and the game Adaptive, the Global tab hides Target, Max and
  Tolerance though the game uses Global's values for them; the Core layer has no frame generation
  rows at all; "Clear game settings" and a layer's Clear leave every `framegen.*` key behind.
- A Global, System or Core video change made over a running game is saved but not applied.
- Integrations drops a staged change without a word if Settings closes before Apply.
- Close game and Clear game settings act on one press; Load state and Reset need two.
- Volume, brightness, interface size and the library options are separate rows on two or three
  screens, worded differently; the Guide menu and the quick menu overlap.
- Choose artwork says to connect SteamGridDB but has no way to get there (18 presses away).
- Raw text reaches the screen: filesystem paths in the launch failure toast, "From
  OPENSU_HOME_MODE" subtitles; About shows no version; toasts are tiny.
- Press counts from Home: a game's shader 31, the global aspect 16, frame generation for one game
  9 (Gamescope only).

### Why: the code has no shared UI layer

- No screen stack. About 25 open flags are read by about 15 hand-ordered chains (input `actOn`, 19
  branches; pointer `pointAtModal`; draw order; `panelOpen`, `menusBlocked`, `hints`, text input
  twice), and the chains disagree on what is on top. A new panel touches about 10 places. Back is
  closures and refocus-by-id, written per screen.
- Every screen hand-rolls its widgets: 12 list focus copies, 11 row structs, 9 scroll copies (none
  eased), 18 pad switches (three near-identical), 7 two-press "armed" copies, about 28 rect-loop
  hit tests that recompute layout apart from drawing, 7 hint-row renderers, 12 focus-ring sites in
  three styles, 81 hand-placed navigation sounds.
- No theme: `outlineDp` defined 9 times, the ink colour literal in 13 files, the scrim in 10.
  Two text-fitting functions disagree on UTF-8 (`Typeface::fitted` can cut a character in half).
- Settings have no schema beyond `ChoiceKey` tables. Inheritance is computed three ways; "clear"
  is hand-listed three times; anything not a `ChoiceKey` (the shader preset) costs 10 to 12 edits
  in 4 files; 82 row-id strings and 10 prefix routers dispatch rows.

## Proposal

### 1. A toolkit: `gpu2d::ui`

Title-neutral, so it lives in gpu2d as its own target beside `gpu2d::render`; openSU supplies the
theme and the screens.

- **Screen stack.** One `Navigator` owns what is open: push, pop, replace. Input, pointer, drawing,
  hints, fades and text input all walk the same stack, top first. Back is pop; a screen can refuse
  or confirm. Modal order exists in one place.
- **Widget tree with layout.** Column, row, stack, grid, scroll view, with padding and spacing.
  Each widget lays out once per frame; painting and hit testing read the same geometry, so pointer
  hits can never drift from what is drawn.
- **One focus model.** Spatial navigation over focusable widgets in the tree (gpu2d's
  `ui/focus`, which the mapping board uses too), focus kept by stable id across rebuilds, scroll-into-view eased.
- **Standard widgets with one behaviour each.** Button, Row (title, subtitle, value), Toggle,
  Choice (Left/Right step; A opens a list), Slider, Tabs (LB/RB), List, Grid, TextField (the
  on-screen keyboard), Dialog, Confirm (one rule for two-press or a dialog), Toast, HintBar built
  from the focused widget's actions, so hints are never hand-written.
- **Theme.** Colour, radius, outline, spacing and type tokens; one focus ring; scale for interface
  size.
- **Motion.** One tween clock: every screen push and pop fades or slides, lists ease.
- **Sounds** come from the toolkit on focus move, activate and back.

### 2. Settings as a schema: `settings::Schema`

One declaration per setting: key, label, help line, type (choice, toggle, slider, path, preset),
choices, default, the layers it may be set at, when it is shown (a condition on other settings'
resolved values), and how a change applies (live, next launch, restart). Everything else is
generated from it: rows, the layer editor, clear, the override count, JSON and validation. Adding a
setting is one declaration plus its consumer.

### 3. Layered settings, redone

- One editor for every scope (Global, a system, a core, a game), reached from Settings or a game.
- Each row shows the value in effect and where it comes from as a chip: "Global", "Game Boy
  Advance", "mGBA", "This game".
- Choosing a value sets it at the scope being edited. The first entry of every list is "Use
  <value> from <layer>"; picking it removes this scope's value. No checkbox. A removed value is
  remembered for the session, so undo is one press.
- Rows show what applies: visibility conditions use the resolved values, not one layer's.
- From a game, a row's secondary action jumps to the layer the value comes from.
- Settings › Emulators: systems and cores as lists, each opening the same editor; sections with
  headings, no 80-row page.
- Core options become schema settings of their core, kept per game like the rest.
- Live changes apply from every editor over a running game.

### 4. Frame generation, redone

- One section in the layered editor everywhere, not only inside Gamescope; outside it a note says
  it takes effect in openSU's Gamescope session.
- A summary row first: "Adaptive · target 60 fps · up to 3x · tolerance 5 fps", then the detail rows
  each with a help line.
- Presets (Off, Smooth 60, Match display, Custom) as the top choice; Custom shows the detail rows.
- The generator source named correctly ("Lossless Scaling's shaders, from your Steam install").

### 5. Order of work

Each step deletes the code it replaces before the next starts.

1. gpu2d: theme, layout, widget tree, focus, `Navigator`, Row, Toggle, Choice, Slider, List, Tabs,
   HintBar, motion. openSU: the Settings screen moved onto it; its old page, panel, painter and
   hit tests deleted.
2. `settings::Schema`; the layered editor and frame generation on it; `layer_settings`,
   `game_settings_page`, `settings_tabs` and their painters deleted.
3. Guide and quick menu merged into one in-game menu on the toolkit; Devices, details, context
   menu, cover picker, downloads, START panel, dialogs and search moved over.
   Done: the Guide menu (one screen; Close game, Load state, Reset game and the power steps ask
   through `confirm`; volume, mute, brightness and interface size only there, Guide + A lands on
   Volume), Devices (X forgets a device after `confirm`), Downloads (gpu2d `Progress`, raised
   `Card`), the details page (`ui::GameDetails` model, cover through `ImageView`, the store's wide
   picture faded behind it through `PictureBackdrop` (the frame's root), Play and the
   action rows through `ui::addRow`, Uninstall asks through `confirm`), the tile context menu (an
   overlay over a scrim; Uninstall asks through `confirm`), the cover picker (an overlay grid of
   gpu2d `CardButton`s, Y signs in to SteamGridDB), a shared row mapping (`ui::addRow`,
   `RowActions`) and page frame (`ui::pageFrame`); `GuideMenu`, `QuickMenu`, their painters and
   `GuidePanels`, `QuickMenuController`, `PagePanel`, `DownloadsPage`/`DownloadsPanel`,
   `DetailsPage`, `ContextMenu`, `CoverPicker`, `CoverPickerArt`, their painters, the two-press
   armed state, `rowEffect` and the Devices, Downloads, details, context menu, cover and backdrop
   pointer targets deleted. Then the START panel (an overlay card: the layout cards as
   `CardButton`s with a `LayoutSketch`, then the rows through `ui::addRow`, each change saved at
   once), the session-mode question, the plugin consent and the install offer as gpu2d `Dialog`s
   (`Dialog::Choice::exits` names the button Back stands for; Steam's licence: Decline), the launch,
   install and session-install cards as one `ui::ProgressScreen` (gpu2d `Progress` with an
   indeterminate sweep), and the first-run integration card (switches through `ui::addRow`, Done);
   `ModeChooser`, `SessionDialog`, `SessionPanels`, `LaunchPanel`, `IntegrationChooser`,
   `SettingsPage`, their painters, `RowEditing`, `rowOpens` and the layout card, options row, icon
   size, panel button, dialog button and integration pointer targets deleted. Then text entry:
   gpu2d `TextField` (masked for a secret), `Action::Erase` and `Submit`, `Navigator::typing` and
   `setKeyboard`; the one on-screen keyboard (`OnScreenKeyboard`, driven by `app::KeyboardPad`)
   became an input method that types into the focused field and holds no line of its own, docked
   under overlays that lay out above it. Search (`search_screen`: the field, the results as items;
   Start goes to them), the folder chooser (`folder_chooser_screen`: the Path field over Use this
   folder, the clear choice, Up one level and the folders), the SteamGridDB key and the
   administrator password (`ui::EntryScreen`) moved over; `SearchPanel`, `SettingsPanels`, the
   search and folder chooser painters, the keyboard's own line, cursor, preview and Clear key, and
   the search result and folder entry pointer targets deleted. Dropped with the keyboard's line:
   the D-pad cursor (a field edits at its end) and the keyboard's Clear key; clearing is now the
   field's own Y (`TextField`'s Tertiary, hinted "Clear" while it holds text), which Select gives
   while the keyboard is up. A hint names only what the device in use can do now (gpu2d
   `Prompts::bound`): a keyboard typing into a field shows Enter for Done and no Type, and a pad
   over the open keyboard leaves the hints to the keyboard's own row.
4. Home grid, rails and dock last: they already ease and work; they join the screen stack.
   Done: text. Every painter (tile, rail, dock, top bar, status pill, breadcrumbs, corner hints,
   toast, volume display, chord list, keyboard, mapping editor, button glyphs) measures and draws
   through the toolkit's `Typesetter`, one face (`ui::InterfaceText`, hinted and kerned) with
   gpu2d `Typesetter::drawCentred` and `sizeForLineHeight`; `TextGrid`, `Typeface`, `FaceMetrics`
   and the dead `row_controls` are deleted, and with them the corner hints' letter spacing.
   Done: Home. `app::HomeScreen` is the navigator's root (`Navigator::setRoot`: never popped, Back
   on it backs out of the search or the folder): one gpu2d `CellGrid` over `ui::HomeGrid`, which
   owns the tiles, the grid's paging and scrolling and the XMB and Carousel rails, their easing
   and painters. The top bar (`ui::TopBarChrome`: breadcrumb levels and launcher badges as parts),
   the dock (`ui::HomeDock`, a gpu2d `TabDock` that LB and RB step from Home), the docked keyboard
   (`ui::KeyboardChrome`: keys as parts) and the mapping editor (`ui::MappingChrome`, which takes
   every press) are chrome screens (`Navigator::addChrome`): the pointer reaches them before the
   top screen and the shell draws each where it belongs, the dock in the second pass over the
   scene its glass blurs. Every button goes through `interfaceActionOf` to `Navigator::act`
   (Select is `Options`, Search is `Search`); the window's pointer and the control channel's go
   through `ShellApp::pointer` to `Navigator::pointer` as Move, Press, Context (a right click,
   never Back), Wheel and Leave. Whether Home is in front is the navigator's `atRoot` and whether a
   screen covers it `active` and `pagesVisible`. Deleted: `Shell::pointAt`, `pointAtHome`,
   `pointAtModal`, `focusTarget`, `moveFocus`, `panelOpen`, `ui::PointerTarget` (`pointer_target.hpp`),
   `PointerRouter` and `PointerHost` with their test, the home switch and `scroll`/`contextMenu` in
   `ShellApp`, `ScreenStack::active`/`visible`/`pagesVisible`/`paged`, and the hit tests the
   widgets' frames replace (`KeyboardLayout::keyAt`, `BreadcrumbLayout::crumbAt`,
   `TopBarLayout::launcherAt`, `dockItemAt`, `onRestingDock`). `tests/app/home_screen_flow_test`
   drives Home through the navigator: focus, paging, LB and RB, A and Y, clicks on tiles and a
   breadcrumb. `/state` keeps `screen` empty on Home.
