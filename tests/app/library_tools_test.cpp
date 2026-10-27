// The search, context menu and options controllers, with the real preferences on a scratch file
// and a silent sound player: the menu's entries per tile and their routing through the interface
// toolkit, hide persistence, Uninstall asking first, icon size and filters.
#include <cstdio>
#include <filesystem>
#include <initializer_list>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "audio/sdl_sound_player.hpp"
#include "context_menu_controller.hpp"
#include "keyboard_pad.hpp"
#include "layout_picker.hpp"
#include "preferences.hpp"
#include "search_controller.hpp"
#include "search_screen.hpp"
#include "support/fake_interface_text.hpp"
#include "ui/check.hpp"
#include "ui/interface_input.hpp"

namespace {

namespace fs = std::filesystem;
using namespace opensu;
using app::ContextMenuController;
using app::LayoutPicker;
using app::Preferences;
using app::SearchController;
using gamepad::Button;
using gpu2d::ui::Action;
using library::LibraryMode;
using test::expect;

library::Game hades() {
    library::Game game;
    game.id = "steam:1";
    game.title = "Hades";
    game.installed = true;
    return game;
}

struct Rig {
    explicit Rig(const fs::path& file)
        : preferences{settings::Store{file}, [this](const std::string& why) {
                          failures.push_back(why);
                      }} {
    }

    audio::SdlSoundPlayer sounds;
    std::vector<std::string> failures;
    Preferences preferences;
};

std::vector<std::string> idsOf(const std::vector<ui::SettingsRow>& rows) {
    std::vector<std::string> ids;
    ids.reserve(rows.size());
    for (const ui::SettingsRow& row : rows) {
        ids.push_back(row.id);
    }
    return ids;
}

std::vector<std::string> ids(std::initializer_list<std::string_view> names) {
    return {names.begin(), names.end()};
}

void menuEntriesFollowTheTile() {
    library::Game installed;
    installed.installed = true;
    const library::Game missing;
    expect(idsOf(app::contextEntriesFor(installed, false)) ==
               ids({app::menuLaunchId, app::menuDetailsId, app::menuHideId, app::menuUninstallId}),
           "an installed store game launches, shows details, hides and uninstalls");
    expect(idsOf(app::contextEntriesFor(missing, false)) ==
               ids({app::menuInstallId, app::menuDetailsId, app::menuHideId}),
           "a missing game installs and has nothing to uninstall");
    expect(
        idsOf(app::contextEntriesFor(installed, true)) ==
            ids({app::menuLaunchId, app::menuDetailsId, app::menuUnhideId, app::menuUninstallId}),
        "a hidden game unhides");
    library::Game rom;
    rom.source = library::Source::Rom;
    rom.installed = true;
    expect(idsOf(app::contextEntriesFor(rom, false)) ==
               ids({app::menuLaunchId, app::menuDetailsId, app::menuHideId}),
           "a ROM is never uninstalled");
    expect(idsOf(app::contextEntriesFor(library::Console{}, false)) ==
               ids({app::menuOpenId, app::menuRefreshId}),
           "a console opens");
    expect(idsOf(app::contextEntriesFor(library::Launcher{library::Source::Gog, 0, false, false},
                                        false)) ==
               ids({app::menuOpenId, app::menuSignInId, app::menuRefreshId}),
           "a GOG launcher without a sign-in offers one");
    expect(idsOf(app::contextEntriesFor(library::Launcher{library::Source::Gog, 3, true, false},
                                        false)) == ids({app::menuOpenId, app::menuRefreshId}),
           "a ready store does not");
    expect(idsOf(app::contextEntriesFor(library::Launcher{library::Source::Steam, 0, false, false},
                                        false)) == ids({app::menuOpenId, app::menuRefreshId}),
           "Steam has no sign-in of its own");
}

void hidingPersistsAndMenuRoutes(const fs::path& root) {
    const fs::path file = root / "menu" / "settings.json";
    Rig rig{file};
    test::InterfaceRig ui;
    library::ShelfItem focus = hades();
    bool noFocus = false;
    std::vector<std::string> log;
    std::string refusal;
    app::UninstallPrompt uninstall{
        {.plan =
             [&](const library::Game&) {
                 return app::Removal{.refusal = refusal, .what = "Steam uninstalls the Steam copy"};
             },
         .start =
             [&](const library::Game& game) {
                 log.push_back("uninstall " + game.id);
                 return std::string{};
             },
         .say =
             [&](const std::string& text, bool error) {
                 log.push_back((error ? "refused " : "said ") + text);
             }}};
    app::UpdateRequest update{{.start =
                                   [&](const library::Game& game) {
                                       log.push_back("update " + game.id);
                                       return std::string{};
                                   },
                               .say =
                                   [&](const std::string& text, bool error) {
                                       log.push_back((error ? "refused " : "said ") + text);
                                   }}};
    ContextMenuController controller{ui.navigator,
                                     rig.sounds,
                                     {rig.preferences, uninstall, update},
                                     {.focused = [&]() -> const library::ShelfItem* {
                                          return noFocus ? nullptr : &focus;
                                      },
                                      .launch =
                                          [&] {
                                              log.emplace_back("launch");
                                          },
                                      .details =
                                          [&] {
                                              log.emplace_back("details");
                                          },
                                      .setHidden =
                                          [&](const library::Game& game, bool hidden) {
                                              rig.preferences.values().hidden.set(game, hidden);
                                              rig.preferences.save();
                                              log.emplace_back(hidden ? "hide" : "unhide");
                                          },
                                      .open =
                                          [&](const library::Folder&) {
                                              log.emplace_back("open");
                                          },
                                      .refresh =
                                          [&] {
                                              log.emplace_back("refresh");
                                          },
                                      .signIn =
                                          [&](library::Source) {
                                              log.emplace_back("signin");
                                          }}};
    const auto open = [&] {
        controller.open();
        ui.frame(1.0);
    };
    const auto press = [&](std::initializer_list<Action> actions) {
        for (const Action action : actions) {
            ui.navigator.act(action);
            ui.frame(1.0);
        }
    };
    const auto top = [&] {
        const gpu2d::ui::Screen* screen = ui.navigator.top();
        return screen == nullptr ? std::string{} : screen->id();
    };

    noFocus = true;
    open();
    expect(!controller.isOpen(), "no focused tile, no menu");
    noFocus = false;
    open();
    expect(controller.isOpen() && top() == "menu" && controller.title() == "Hades" &&
               controller.entries().size() == 4 && ui.navigator.focused() == app::menuLaunchId,
           "a game's menu, on its first entry");
    press({Action::Back});
    expect(!controller.isOpen() && log.empty(), "B closes without acting");

    open();
    press({Action::Down, Action::Down, Action::Accept});
    expect(log == std::vector<std::string>{"hide"} && !controller.isOpen(),
           "A runs Hide and closes");
    expect(settings::Store{file}.load().hidden.contains(hades()), "the hidden game is on disk");

    open();
    expect(controller.entries()[2].id == app::menuUnhideId, "a hidden game offers Unhide");
    press({Action::Down, Action::Down, Action::Accept});
    expect(log.back() == "unhide" && !settings::Store{file}.load().hidden.contains(hades()),
           "Unhide is kept too");

    open();
    press({Action::Accept});
    expect(log.back() == "launch", "the first entry launches an installed game");
    open();
    press({Action::Down, Action::Accept});
    expect(log.back() == "details", "the second shows details");

    log.clear();
    open();
    press({Action::Down, Action::Down, Action::Down, Action::Accept});
    expect(!controller.isOpen() && top() == "dialog" &&
               ui.navigator.top()->title() == "Uninstall Hades?" && log.empty(),
           "Uninstall asks first, naming the game, and does nothing yet");
    press({Action::Back});
    expect(ui.navigator.top() == nullptr && log.empty(), "Back keeps the game");
    open();
    press({Action::Down, Action::Down, Action::Down, Action::Accept, Action::Accept});
    expect(log == std::vector<std::string>{"uninstall steam:1", "said uninstalling Hades"} &&
               ui.navigator.top() == nullptr,
           "A, A uninstalls the menu's game");
    log.clear();
    refusal = "Hades is running";
    open();
    press({Action::Down, Action::Down, Action::Down, Action::Accept});
    expect(log == std::vector<std::string>{"refused cannot uninstall Hades: Hades is running"} &&
               ui.navigator.top() == nullptr,
           "a refused Uninstall says why and asks nothing");

    log.clear();
    auto* updatable = std::get_if<library::Game>(&focus);
    expect(updatable != nullptr, "the menu is still over Hades");
    updatable->updateAvailable = true;
    open();
    expect(controller.entries().size() == 5 && controller.entries()[1].id == app::menuUpdateId &&
               controller.entries()[4].id == app::menuUninstallId,
           "a game with an update offers Update after Launch");
    press({Action::Down, Action::Accept});
    expect(log == std::vector<std::string>{"update steam:1", "said updating Hades"} &&
               !controller.isOpen(),
           "Update starts the menu's game's update at once and closes the menu");

    focus = library::Launcher{library::Source::Gog, 0, false, false};
    open();
    press({Action::Down, Action::Accept});
    expect(log.back() == "signin", "sign in routes to the store");
    focus = library::Console{"gc", "GameCube", 4, {}};
    open();
    expect(controller.title() == "GameCube", "a folder's menu is titled with its name");
    press({Action::Accept});
    expect(log.back() == "open", "a console's Open opens its folder");
    expect(rig.failures.empty(), "nothing failed to save");
}

void searchTypesAndCloses(const fs::path& root) {
    Rig rig{root / "search" / "settings.json"};
    test::InterfaceRig ui;
    ui::Chrome chrome;
    ui::OnScreenKeyboard keyboard;
    keyboard.setStyle(input::KeyboardStyle::Regular);
    app::KeyboardPad pad{keyboard, ui.navigator, rig.sounds};
    ui.navigator.setKeyboard([&pad] {
        pad.open();
    });
    std::size_t reshown = 0;
    std::vector<std::size_t> opened;
    SearchController controller{SearchController::Parts{.navigator = ui.navigator,
                                                        .chrome = chrome,
                                                        .keyboard = pad,
                                                        .sounds = rig.sounds,
                                                        .preferences = rig.preferences},
                                {.reshow =
                                     [&](std::size_t) {
                                         ++reshown;
                                     },
                                 .results =
                                     [&] {
                                         return std::vector<app::SearchResult>{{"Hades", "Steam"},
                                                                               {"Hadean", "GOG"}};
                                     },
                                 .openTile =
                                     [&](std::size_t index) {
                                         opened.push_back(index);
                                     }}};
    const auto frame = [&] {
        ui.frame(1.0);
        pad.follow();
    };
    const auto act = [&](Action action) {
        ui.navigator.act(action);
        frame();
    };
    const auto button = [&](Button pressed) {
        pad.act(pressed);
        frame();
    };

    controller.open();
    frame();
    expect(controller.isOpen() && !controller.searching() && pad.isOpen() &&
               ui.navigator.focused() == app::SearchScreen::fieldId,
           "opens on the field with nothing searched and the keyboard up");
    ui.navigator.text("ha");
    frame();
    expect(controller.searching() && rig.preferences.values().view.search == "ha",
           "typed text is the view's search");
    expect(controller.results().size() == 2 && reshown >= 2 &&
               ui.navigator.find(app::SearchScreen::resultId(1)) != nullptr,
           "results are listed as rows and the grid reshown");
    act(Action::Erase);
    expect(rig.preferences.values().view.search == "h", "Backspace narrows back");
    ui.navigator.text(" ");
    ui.navigator.text(" ");
    expect(rig.preferences.values().view.search == "h ", "a space types, but never twice");
    act(Action::Submit);
    expect(ui.navigator.focused() == app::SearchScreen::resultId(0) && !pad.isOpen(),
           "Submit goes to the first result, and the keyboard closes off the field");
    act(Action::Down);
    act(Action::Accept);
    frame();
    expect(opened == std::vector<std::size_t>{1} && !controller.isOpen(),
           "A opens the focused result and closes the card");
    expect(controller.searching(), "closing leaves the search applied");
    expect(controller.clear() && !controller.searching() && !controller.clear(),
           "B clears the search once");

    controller.open();
    frame();
    button(Button::A);
    expect(rig.preferences.values().view.search == "1", "A types the focused key");
    button(Button::B);
    expect(rig.preferences.values().view.search.empty(), "B deletes a character");
    button(Button::B);
    expect(!pad.isOpen() && controller.isOpen(), "B on an empty field closes the keyboard");
    expect(ui::interfaceActionOf(Button::Start, ui.navigator.typing()) == Action::Submit &&
               ui::interfaceActionOf(Button::Start, false) == Action::Menu,
           "Start is Done on the field, the menu button elsewhere");
    act(Action::Back);
    expect(!controller.isOpen(), "and B again the card");

    controller.open();
    frame();
    ui.navigator.text("x");
    act(Action::Back);
    expect(!controller.isOpen() && !pad.isOpen() && controller.searching(),
           "Escape closes, the search kept");
    controller.clear();

    // The daisy wheel: the stick's petal and the face buttons type; Done goes to the results.
    keyboard.setStyle(input::KeyboardStyle::DaisyWheel);
    controller.open();
    frame();
    pad.stick(ui::Direction::Right, true);
    button(Button::Y);
    button(Button::A);
    expect(rig.preferences.values().view.search == "jl" && controller.results().size() == 2,
           "a petal's characters are the search");
    pad.stick(ui::Direction::Right, false);
    button(Button::X);
    expect(rig.preferences.values().view.search == "j", "with no petal X deletes");
    button(Button::Left);
    ui.navigator.text("a");
    button(Button::Right);
    ui.navigator.text("b");
    expect(rig.preferences.values().view.search == "ajb" && pad.isOpen() &&
               ui.navigator.focused() == app::SearchScreen::fieldId,
           "the wheel's D-pad moves the caret, and typing goes in at it");
    button(Button::Start);
    expect(!pad.isOpen() && ui.navigator.focused() == app::SearchScreen::resultId(0),
           "Start is Done: the keyboard closes on the first result");
    act(Action::Up);
    act(Action::Accept);
    expect(pad.isOpen(), "A on the field brings the keyboard back");
    button(Button::B);
    expect(!pad.isOpen() && controller.isOpen() && controller.searching(),
           "B with no petal closes the keyboard, the search kept");
}

void optionsChangeAndKeep(const fs::path& root) {
    const fs::path file = root / "options" / "settings.json";
    Rig rig{file};
    test::InterfaceRig ui;
    std::size_t reshown = 0;
    std::size_t applied = 0;
    bool searchOpened = false;
    bool settingsOpened = false;
    LayoutPicker picker{
        ui.navigator,
        rig.sounds,
        rig.preferences,
        {.reshow =
             [&](std::size_t) {
                 ++reshown;
             },
         .focusIndex =
             [] {
                 return std::size_t{0};
             },
         .applyLayout =
             [&] {
                 ++applied;
             },
         .sourceChoices =
             [] {
                 return std::vector<library::SourceChoice>{
                     {"", "All sources"}, {"launcher:epic", "Epic"}, {"gc", "GameCube"}};
             },
         .openSearch =
             [&] {
                 searchOpened = true;
             },
         .openSettings =
             [&] {
                 settingsOpened = true;
             }}};
    const auto open = [&](bool inLibrary) {
        picker.open(inLibrary);
        ui.frame(1.0);
    };
    const auto press = [&](std::initializer_list<Action> actions) {
        for (const Action action : actions) {
            ui.navigator.act(action);
            ui.frame(1.0);
        }
    };
    const auto at = [&](std::string_view id) {
        ui.navigator.focus(id);
        ui.frame(1.0);
    };

    open(true);
    expect(picker.isOpen() && ui.navigator.focused() == app::layoutCardId(LibraryMode::Standard),
           "Library's panel opens on the layout in use");
    press({Action::Right});
    expect(ui.navigator.focused() == app::layoutCardId(LibraryMode::Xmb),
           "Right moves to the next card");
    press({Action::Down});
    expect(ui.navigator.focused() == app::optionIconSizeId, "Down leaves the cards for the rows");
    press({Action::Right, Action::Right});
    expect(rig.preferences.values().iconSize == 11 && applied == 2 &&
               settings::Store{file}.load().iconSize == 11,
           "Right grows the icons, kept at once");
    picker.setLevel(std::string{app::optionIconSizeId}, 99);
    expect(rig.preferences.values().iconSize == settings::maxIconSize, "the slider clamps");
    press({Action::Back});
    expect(!picker.isOpen(), "B closes it");

    open(true);
    press({Action::Right, Action::Accept});
    expect(!picker.isOpen() && rig.preferences.values().libraryMode == LibraryMode::Xmb &&
               settings::Store{file}.load().libraryMode == LibraryMode::Xmb && reshown == 1,
           "A on a card uses its layout, keeps it and closes the panel");

    open(true);
    at(app::optionSortId);
    press({Action::Right});
    expect(rig.preferences.values().view.sort == library::SortKey::Name, "Right steps the sort");
    press({Action::Left});
    expect(rig.preferences.values().view.sort == library::SortKey::Recent, "Left steps back");
    at(app::optionSourceId);
    press({Action::Right});
    expect(rig.preferences.values().view.source == "launcher:epic",
           "Right moves to the next source");
    press({Action::Left, Action::Left});
    expect(rig.preferences.values().view.source == "gc", "Left wraps to the last source");
    at(app::optionInstalledId);
    press({Action::Accept});
    at(app::optionHiddenId);
    press({Action::Accept});
    const settings::Settings saved = settings::Store{file}.load();
    expect(saved.view.installedOnly && saved.view.source == "gc" && !saved.view.hiddenOnly,
           "the filters are kept, the hidden filter is not");
    expect(rig.preferences.values().view.hiddenOnly && reshown >= 6,
           "the hidden filter applies now");
    at(app::optionPinId);
    press({Action::Accept});
    expect(!rig.preferences.values().pinLibraryDock, "A toggles the pin");
    at(app::optionSearchId);
    press({Action::Accept});
    expect(!picker.isOpen() && searchOpened, "the Search row closes the panel and opens search");

    open(true);
    at(app::optionSettingsId);
    press({Action::Accept});
    expect(!picker.isOpen() && settingsOpened,
           "the Settings row closes the panel and opens the Settings screen");

    open(false);
    expect(ui.navigator.focused() == app::optionSortId &&
               ui.navigator.find(app::layoutCardId(LibraryMode::Standard)) == nullptr &&
               ui.navigator.find(app::optionIconSizeId) == nullptr,
           "Home's panel has no layout rows and opens on the sort");
    press({Action::Menu});
    expect(!picker.isOpen(), "START closes it");
    expect(rig.failures.empty(), "nothing failed to save");
}

} // namespace

int main() {
    const fs::path root = fs::path{OPENSU_TEST_SCRATCH} / "library_tools";
    fs::remove_all(root);
    menuEntriesFollowTheTile();
    hidingPersistsAndMenuRoutes(root);
    searchTypesAndCloses(root);
    optionsChangeAndKeep(root);
    fs::remove_all(root);
    std::printf("library_tools: all checks passed\n");
    return 0;
}
