// The search, context menu and options controllers, with the real preferences on a scratch file
// and a silent sound player: routing of every button, hide persistence, icon size and filters.
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

#include "audio/sound_player.hpp"
#include "context_menu_controller.hpp"
#include "layout_picker.hpp"
#include "preferences.hpp"
#include "search_controller.hpp"
#include "ui/check.hpp"

namespace {

namespace fs = std::filesystem;
using namespace opensu;
using app::ContextMenuController;
using app::LayoutPicker;
using app::Preferences;
using app::SearchController;
using gamepad::Button;
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

    audio::SoundPlayer sounds;
    std::vector<std::string> failures;
    Preferences preferences;
};

void hidingPersistsAndMenuRoutes(const fs::path& root) {
    const fs::path file = root / "menu" / "settings.json";
    Rig rig{file};
    ui::ContextMenu menu;
    library::ShelfItem focus = hades();
    bool noFocus = false;
    std::vector<std::string> log;
    ContextMenuController controller{menu,
                                     rig.sounds,
                                     rig.preferences,
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

    noFocus = true;
    controller.open();
    expect(!menu.isOpen(), "no focused tile, no menu");
    noFocus = false;
    controller.open();
    expect(menu.isOpen() && menu.title() == "Hades" && menu.items().size() == 3, "a game's menu");
    controller.act(Button::B);
    expect(!menu.isOpen() && log.empty(), "B closes without acting");

    controller.open();
    controller.act(Button::Select);
    expect(!menu.isOpen(), "Select closes it too");

    controller.open();
    controller.act(Button::Down);
    controller.act(Button::Down);
    controller.act(Button::A);
    expect(log == std::vector<std::string>{"hide"} && !menu.isOpen(), "A runs Hide and closes");
    expect(settings::Store{file}.load().hidden.contains(hades()), "the hidden game is on disk");

    controller.open();
    expect(menu.items().back().action == ui::ContextAction::Unhide, "a hidden game offers Unhide");
    controller.act(Button::Down);
    controller.act(Button::Down);
    controller.act(Button::A);
    expect(log.back() == "unhide" && !settings::Store{file}.load().hidden.contains(hades()),
           "Unhide is kept too");

    controller.open();
    controller.act(Button::A);
    expect(log.back() == "launch", "the first entry launches an installed game");
    controller.open();
    controller.act(Button::Down);
    controller.act(Button::A);
    expect(log.back() == "details", "the second shows details");

    focus = library::Launcher{library::Source::Gog, 0, false, false};
    controller.open();
    expect(menu.items().size() == 3 && menu.items()[1].action == ui::ContextAction::SignIn,
           "a GOG launcher offers sign in");
    controller.act(Button::Down);
    controller.act(Button::A);
    expect(log.back() == "signin", "sign in routes to the store");
    focus = library::Console{"gc", "GameCube", 4, {}};
    controller.open();
    controller.act(Button::A);
    expect(log.back() == "open", "a console's Open opens its folder");
    expect(rig.failures.empty(), "nothing failed to save");
}

void searchTypesAndCloses(const fs::path& root) {
    Rig rig{root / "search" / "settings.json"};
    ui::SearchPanel panel;
    std::size_t reshown = 0;
    std::vector<std::size_t> opened;
    SearchController controller{panel,
                                rig.sounds,
                                rig.preferences,
                                {.reshow =
                                     [&](std::size_t) {
                                         ++reshown;
                                     },
                                 .results =
                                     [&] {
                                         return std::vector<ui::SearchResult>{{"Hades", "Steam"},
                                                                              {"Hadean", "GOG"}};
                                     },
                                 .openTile =
                                     [&](std::size_t index) {
                                         opened.push_back(index);
                                     }}};

    controller.open();
    expect(panel.isOpen() && !controller.searching(), "opens with nothing searched");
    controller.typeText("ha");
    expect(controller.searching() && rig.preferences.values().view.search == "ha",
           "typed text is the view's search");
    expect(panel.results().size() == 2 && reshown >= 2, "results are listed and the grid reshown");
    controller.backspace();
    expect(rig.preferences.values().view.search == "h", "backspace narrows back");
    controller.act(Button::Up);
    expect(panel.zone() == ui::SearchZone::Results, "up from the keys enters the results");
    controller.confirm();
    expect(opened == std::vector<std::size_t>{0} && !panel.isOpen(),
           "Enter opens the focused result");
    expect(controller.searching(), "closing leaves the search applied");
    expect(controller.clear() && !controller.searching() && !controller.clear(),
           "B clears the search once");

    controller.open();
    controller.act(Button::A);
    expect(panel.text() == "1", "A types the focused key");
    controller.act(Button::B);
    expect(panel.text().empty(), "B deletes a character");
    controller.act(Button::B);
    expect(!panel.isOpen(), "B on an empty field closes");

    controller.open();
    controller.typeText("x");
    controller.act(Button::Select);
    expect(panel.text().empty() && !controller.searching(), "Select clears the field");
    controller.dismiss();
    expect(!panel.isOpen(), "Escape closes");
    controller.open();
    controller.confirm();
    expect(!panel.isOpen() && opened.size() == 1, "Enter on the keys only closes");
}

void optionsChangeAndKeep(const fs::path& root) {
    const fs::path file = root / "options" / "settings.json";
    Rig rig{file};
    ui::ModeChooser chooser;
    std::size_t reshown = 0;
    std::size_t applied = 0;
    bool searchOpened = false;
    LayoutPicker picker{
        chooser,
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
             }}};

    picker.open(true);
    expect(chooser.isOpen() && chooser.values().iconSize == 9, "opens at the default size");
    chooser.focusRow(ui::ChooserRow::IconSize);
    picker.act(Button::Right);
    picker.act(Button::Right);
    expect(rig.preferences.values().iconSize == 11 && applied == 2, "right grows the icons");
    expect(settings::Store{file}.load().iconSize == 9, "the size is kept when the panel closes");
    picker.chooseIconSize(99);
    expect(rig.preferences.values().iconSize == settings::maxIconSize, "the slider clamps");
    picker.act(Button::B);
    expect(!chooser.isOpen() && settings::Store{file}.load().iconSize == settings::maxIconSize,
           "closing saves the icon size");

    picker.open(true);
    chooser.focusRow(ui::ChooserRow::Sort);
    picker.act(Button::A);
    expect(rig.preferences.values().view.sort == library::SortKey::Name &&
               chooser.values().sort == "Name",
           "A cycles the sort");
    picker.act(Button::Left);
    expect(rig.preferences.values().view.sort == library::SortKey::Recent, "left cycles back");
    chooser.focusRow(ui::ChooserRow::Source);
    picker.act(Button::A);
    expect(rig.preferences.values().view.source == "launcher:epic" &&
               chooser.values().source == "Epic",
           "A moves to the next source");
    picker.act(Button::Left);
    picker.act(Button::Left);
    expect(rig.preferences.values().view.source == "gc", "left wraps to the last source");
    chooser.focusRow(ui::ChooserRow::Installed);
    picker.act(Button::A);
    chooser.focusRow(ui::ChooserRow::Hidden);
    picker.act(Button::A);
    const settings::Settings saved = settings::Store{file}.load();
    expect(saved.view.installedOnly && saved.view.source == "gc" && !saved.view.hiddenOnly,
           "the filters are kept, the hidden filter is not");
    expect(rig.preferences.values().view.hiddenOnly && reshown >= 5,
           "the hidden filter applies now");
    chooser.focusRow(ui::ChooserRow::Pin);
    picker.act(Button::A);
    expect(!rig.preferences.values().pinLibraryDock, "A toggles the pin");
    chooser.focusRow(ui::ChooserRow::Search);
    picker.act(Button::A);
    expect(!chooser.isOpen() && searchOpened, "the Search row closes the panel and opens search");

    picker.open(false);
    expect(chooser.rows().size() < 8, "Home's panel holds fewer rows");
    picker.act(Button::Start);
    expect(!chooser.isOpen(), "Start closes it");
    expect(rig.failures.empty(), "nothing failed to save");
}

} // namespace

int main() {
    const fs::path root = fs::path{OPENSU_TEST_SCRATCH} / "library_tools";
    fs::remove_all(root);
    hidingPersistsAndMenuRoutes(root);
    searchTypesAndCloses(root);
    optionsChangeAndKeep(root);
    fs::remove_all(root);
    std::printf("library_tools: all checks passed\n");
    return 0;
}
