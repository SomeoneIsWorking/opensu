// The in-game Guide menu's state.
#include "game_menu.hpp"

#include <cstdio>

#include "check.hpp"

namespace {

using opensu::test::expect;
using opensu::test::near;
using opensu::ui::GameMenu;
using opensu::ui::GameMenuAction;
using opensu::ui::GameMenuLayout;

void opensOnResume() {
    GameMenu menu;
    expect(!menu.isOpen(), "starts closed");
    menu.open("Celeste");
    expect(menu.isOpen(), "opens");
    expect(menu.title() == "Celeste", "keeps the game's title");
    expect(menu.selected() == GameMenuAction::Resume, "Resume is focused first");
}

void movesAndStops() {
    GameMenu menu;
    menu.open("Celeste");
    menu.move(-1);
    expect(menu.focus() == 0, "stops at the top");
    menu.move(1);
    expect(menu.selected() == GameMenuAction::CloseGame, "Close game is below Resume");
    menu.move(5);
    expect(menu.focus() == menu.items().size() - 1, "stops at the bottom");
}

void reopenResetsFocus() {
    GameMenu menu;
    menu.open("Celeste");
    menu.move(1);
    menu.close();
    expect(!menu.isOpen(), "closes");
    menu.open("Celeste");
    expect(menu.selected() == GameMenuAction::Resume, "a reopened menu starts on Resume");
}

void rowsUnderThePointer() {
    const GameMenuLayout layout = opensu::ui::layoutGameMenu(1920.0f, 1080.0f, 2.25f, 40.0f);
    near(layout.panel.width, 675.0, "the panel is 300 dp wide");
    near(layout.panel.height, 1080.0, "and as tall as the frame");
    near(layout.rows[0].y, 54.0 + 40.0 + 54.0, "the rows start below the title");
    near(layout.rows[1].y - layout.rows[0].bottom(), 13.5, "6 dp apart");
    for (std::size_t i = 0; i < layout.rows.size(); ++i) {
        const auto hit = layout.itemAt(layout.rows[i].centreX(), layout.rows[i].centreY());
        expect(hit && *hit == i, "a row's centre hits it");
    }
    expect(!layout.itemAt(layout.rows[0].centreX(), layout.rows[0].bottom() + 5.0f),
           "the gap between rows hits none");
    expect(!layout.itemAt(layout.panel.right() + 10.0f, layout.rows[0].centreY()),
           "outside the panel hits none");
}

void focusByPointer() {
    GameMenu menu;
    menu.open("Celeste");
    expect(menu.focusItem(1) && menu.selected() == GameMenuAction::CloseGame, "a row takes focus");
    expect(!menu.focusItem(1), "the focused row is not a move");
    expect(!menu.focusItem(9) && menu.focus() == 1, "a row that is not there is ignored");
}

} // namespace

int main() {
    rowsUnderThePointer();
    focusByPointer();
    opensOnResume();
    movesAndStops();
    reopenResetsFocus();
    std::printf("game_menu: all checks passed\n");
    return 0;
}
