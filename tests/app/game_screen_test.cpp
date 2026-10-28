// The window over a running game: the chord list shows it without taking the pads, a menu takes
// them, and closing the menu under the list gives them back.
#include <cstdio>
#include <vector>

#include "game_screen.hpp"
#include "ui/check.hpp"

namespace {

using opensu::app::GameScreen;
using opensu::display::OverlayMode;
using opensu::test::expect;

void theChordListLeavesThePadsToTheGame() {
    std::vector<bool> blocks;
    GameScreen screen{true, [&blocks](bool blocked) {
                          blocks.push_back(blocked);
                      }};
    screen.setRunning(true);
    expect(screen.mode() == OverlayMode::Hidden, "nothing is shown over a game that starts");
    screen.show(OverlayMode::Passive);
    expect(screen.mode() == OverlayMode::Passive && blocks.empty(),
           "the chord list shows without blocking the pads");
    screen.show(OverlayMode::Interactive);
    expect(blocks == std::vector{true}, "a menu blocks them");
    screen.show(OverlayMode::Passive);
    expect(blocks == std::vector{true, false}, "back to the chord list, they reach the game again");
    screen.show(OverlayMode::Passive);
    screen.show(OverlayMode::Hidden);
    expect(screen.mode() == OverlayMode::Hidden && blocks.size() == 2,
           "a repeat changes nothing, and hiding the list leaves the pads alone");
    screen.show(OverlayMode::Interactive);
    screen.setRunning(false);
    expect(screen.mode() == OverlayMode::Hidden, "a game that ends shows nothing over it");
}

} // namespace

int main() {
    theChordListLeavesThePadsToTheGame();
    std::printf("game_screen: all checks passed\n");
    return 0;
}
