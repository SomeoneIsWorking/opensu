// When a hosted game nobody plays any more closes (`GameLeftEmpty`).
#include "app/game_left_empty.hpp"

#include <cstdio>
#include <cstdlib>

namespace {

void expect(bool condition, const char* what) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        std::exit(1);
    }
}

} // namespace

int main() {
    using opensu::app::GameLeftEmpty;
    {
        GameLeftEmpty game;
        game.left(true);
        expect(game.closes(false, 0), "a taken-over seat left cleanly with no one in closes");
    }
    {
        GameLeftEmpty game;
        game.left(true);
        expect(!game.closes(false, 1), "a guest still in keeps it going");
        game.left(false);
        expect(game.closes(false, 0), "and when that guest leaves too, it closes");
    }
    {
        GameLeftEmpty game;
        game.left(false);
        expect(!game.closes(false, 0), "the player here still plays their own game");
    }
    {
        const GameLeftEmpty game;
        expect(game.closes(true, 0) && !game.closes(true, 1),
               "a game started for a guest closes when its last guest leaves");
    }
    std::printf("game_left_empty: all checks passed\n");
    return 0;
}
