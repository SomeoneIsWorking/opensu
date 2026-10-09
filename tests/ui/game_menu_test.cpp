// The in-game Guide menu's state.
#include "game_menu.hpp"

#include <cstdio>

#include "check.hpp"

namespace {

using opensu::test::expect;
using opensu::ui::GameMenu;
using opensu::ui::GameMenuAction;

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

} // namespace

int main() {
    opensOnResume();
    movesAndStops();
    reopenResetsFocus();
    std::printf("game_menu: all checks passed\n");
    return 0;
}
