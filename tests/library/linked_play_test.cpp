// A game on a core that links its players: what makes it linkable and the command each way of
// playing gets.
#include "library/linked_play.hpp"

#include <algorithm>
#include <cstdio>

#include "check.hpp"
#include "support/test_main.hpp"

namespace {

using opensu::library::EmulatorOption;
using opensu::library::Game;
using opensu::test::expect;
namespace library = opensu::library;

Game gbaGame() {
    Game game;
    game.source = opensu::library::Source::Rom;
    game.installed = true;
    game.emulator = "mGBA";
    game.launch.args = {"--core", "mgba.so", "rom.gba"};
    EmulatorOption official;
    official.name = "mGBA";
    official.id = "mgba-core";
    official.installed = true;
    EmulatorOption link;
    link.name = "mGBA link";
    link.id = "mgba-core";
    link.installed = true;
    link.links = true;
    link.launch.program = "opensu-retro";
    link.launch.args = {"--core", "link.so", "rom.gba"};
    game.emulatorOptions = {official, link};
    return game;
}

bool holds(const Game& game, std::string_view argument) {
    return std::ranges::find(game.launch.args, argument) != game.launch.args.end();
}

void linkableNeedsTheCore() {
    Game game = gbaGame();
    expect(library::linkable(game), "a ROM with the link core installed");
    game.emulatorOptions[1].installed = false;
    expect(!library::linkable(game), "not without it");
    expect(!library::splitScreenGame(game), "and no split screen");
    game = gbaGame();
    game.emulatorOptions[1].problem = "the runner is missing";
    expect(!library::linkable(game), "nor when it cannot start");
    game = gbaGame();
    game.installed = false;
    expect(!library::linkable(game), "nor when the game is not here");
    game = gbaGame();
    game.emulatorOptions[1].links = false;
    expect(!library::linkable(game), "nor on a core that does not link this system");
}

void splitScreen() {
    const Game game = opensu::test::need(library::splitScreenGame(gbaGame()), "a linked game");
    expect(game.emulator == "mGBA link", "the link core is picked");
    expect(game.launch.program == "opensu-retro" && holds(game, "link.so"), "and its command");
    expect(holds(game, "--players") && holds(game, "2"), "two players");
    expect(holds(game, "mgba_multiplayer_splitscreen=Side by Side"), "side by side");
    expect(game.launch.args.back() == "rom.gba", "the ROM stays last");
}

void netplay() {
    const Game game =
        opensu::test::need(library::netplayGame(gbaGame(), "session.json"), "a session's game");
    expect(holds(game, "--netplay") && holds(game, "session.json"), "the session's file");
    expect(holds(game, "mgba_multiplayer_av=Self"), "only this machine's player shows");
    expect(!holds(game, "--players"), "the session says how many play");
}

} // namespace

void anyBuiltInCore() {
    Game game = gbaGame();
    game.emulatorOptions.clear();
    expect(!linkable(game) && netplayable(game), "a game on a built-in core can be a session");
    expect(netplayCore(game) == "mgba.so", "on its own core");
    const std::optional<Game> played = netplayGame(game, "session.json");
    expect(played && holds(*played, "--netplay") && holds(*played, "mgba.so"),
           "the runner takes the session on that core");
    expect(sessionCore(game) == "mgba.so", "a session runs that core");
    game.launch.args = {"rom.gba"};
    expect(!netplayable(game), "an emulator of its own cannot");
    game = gbaGame();
    expect(netplayCore(game).empty(), "the link core comes first where it can");
    expect(sessionCore(game) == "link.so", "and a session runs it");
}

int main() try {
    anyBuiltInCore();
    linkableNeedsTheCore();
    splitScreen();
    netplay();
    std::printf("linked_play: all checks passed\n");
    return 0;
} catch (...) {
    return opensu::test::escaped();
}
