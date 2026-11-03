// The import pick list through the navigator as the pad presses it: every game chosen at first, a
// switch per game, Select all, Deselect all, Undo of each change, and Import selected handing over
// only the games chosen.
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "import_picks_screen.hpp"
#include "support/fake_interface_text.hpp"
#include "support/test_main.hpp"
#include "ui/check.hpp"

namespace {

using namespace opensu;
using gpu2d::ui::Action;
using library::own::Entry;
using test::expect;

std::vector<Entry> found() {
    std::vector<Entry> games;
    for (const char* title : {"Bloodborne", "ELDEN RING", "Lies of P"}) {
        games.push_back(Entry{.id = {},
                              .title = title,
                              .file = {},
                              .origin = library::own::Origin::Lutris,
                              .originId = title});
    }
    return games;
}

struct Rig {
    test::InterfaceRig ui;
    app::ImportPicksScreen* screen{};
    std::vector<std::string> said;
    std::vector<Entry> imported;
    bool importedOnce{false};

    Rig() {
        auto owned = std::make_unique<app::ImportPicksScreen>(
            "Import from Lutris", app::ImportPicks{found()}, "/home/player",
            app::ImportPicksScreen::Hooks{.say =
                                              [this](const std::string& text, bool /*error*/) {
                                                  said.push_back(text);
                                              },
                                          .imported =
                                              [this](std::vector<Entry> picked) {
                                                  imported = std::move(picked);
                                                  importedOnce = true;
                                              }});
        screen = owned.get();
        ui.navigator.push(std::move(owned));
        ui.frame(1.0);
    }
    void press(std::string_view id) {
        ui.navigator.focus(id);
        ui.frame(1.0);
        ui.navigator.act(Action::Accept);
        ui.frame(1.0);
    }
    [[nodiscard]] std::size_t chosen() const {
        return screen->picks().chosenCount();
    }
};

void everyGameStartsChosen() {
    Rig rig;
    expect(rig.chosen() == 3 && rig.ui.navigator.focused() == app::importAllId,
           "every game found is chosen and Select all is focused");
    rig.press(app::importDoneId);
    expect(rig.importedOnce && rig.imported.size() == 3 && rig.ui.navigator.top() == nullptr,
           "Import selected hands over all of them and closes the list");
}

void switchesAndUndo() {
    Rig rig;
    rig.press(app::importNoneId);
    expect(rig.chosen() == 0, "Deselect all leaves every game out");
    rig.press(app::importGameId(0));
    rig.press(app::importGameId(2));
    expect(rig.chosen() == 2 && rig.screen->picks().chosen(2), "A on a game's switch chooses it");
    rig.press(app::importUndoId);
    expect(rig.chosen() == 1 && !rig.screen->picks().chosen(2), "Undo takes back the last change");
    rig.press(app::importUndoId);
    rig.press(app::importUndoId);
    expect(rig.chosen() == 3, "Undo again goes back before Deselect all");
    rig.press(app::importUndoId);
    expect(rig.said == std::vector<std::string>{"Nothing to undo"}, "with no change left, says so");
    rig.press(app::importNoneId);
    rig.press(app::importGameId(1));
    rig.press(app::importDoneId);
    expect(rig.imported.size() == 1 && rig.imported.front().title == "ELDEN RING",
           "only the game chosen is imported");
}

void nothingChosenImportsNothing() {
    Rig rig;
    rig.press(app::importNoneId);
    rig.press(app::importDoneId);
    expect(!rig.importedOnce && rig.ui.navigator.top() == rig.screen &&
               rig.said == std::vector<std::string>{"Choose a game to import first"},
           "with nothing chosen Import selected says so and stays");
    rig.press(app::importAllId);
    expect(rig.chosen() == 3, "Select all chooses every game");
    rig.ui.navigator.act(Action::Back);
    rig.ui.frame(1.0);
    expect(!rig.importedOnce && rig.ui.navigator.top() == nullptr,
           "Back closes it importing nothing");
}

} // namespace

int main() try {
    everyGameStartsChosen();
    switchesAndUndo();
    nothingChosenImportsNothing();
    std::printf("import_picks: all checks passed\n");
    return 0;
} catch (...) {
    return opensu::test::escaped();
}
