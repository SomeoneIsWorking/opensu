// The emulator picked for a ROM: it points the game's launch at the pick, and a pick that is not
// installed says so.
#include "library/emulator_choice.hpp"

#include <cstdio>
#include <vector>

#include "ui/check.hpp"

namespace {

using namespace opensu::library;
using opensu::test::expect;

Game kirby() {
    Game game;
    game.id = "rom:/r/Kirby.nsp";
    game.source = Source::Rom;
    game.installed = true;
    game.emulatorOptions = {
        EmulatorOption{"Eden", true, LaunchSpec{"eden", {"/r/Kirby.nsp"}}},
        EmulatorOption{"Ryujinx", true, LaunchSpec{"ryujinx", {"/r/Kirby.nsp"}}},
        EmulatorOption{"Yuzu", false, {}},
    };
    game.emulator = "Eden";
    game.launch = game.emulatorOptions.front().launch;
    return game;
}

void pickPointsLaunchAtTheEmulator() {
    std::vector<Game> games{kirby()};
    EmulatorChoices choices;
    choices.apply(games);
    expect(games[0].emulator == "Eden", "a game with no pick keeps the search's emulator");

    choices.set(games[0].id, "Ryujinx");
    choices.apply(games);
    expect(games[0].emulator == "Ryujinx" && games[0].launch.program == "ryujinx" &&
               games[0].unavailable.empty(),
           "a pick runs the game with that emulator");

    choices.set(games[0].id, "Yuzu");
    choices.apply(games);
    expect(games[0].launch.empty() && games[0].unavailable == "Yuzu is not installed",
           "a pick that is not installed cannot launch and says so");

    choices.set(games[0].id, "Eden");
    choices.apply(games);
    expect(games[0].launch.program == "eden" && games[0].unavailable.empty(),
           "picking an installed emulator again launches");
}

void unknownPickIsIgnored() {
    std::vector<Game> games{kirby()};
    EmulatorChoices choices{{{games[0].id, "Gone"}, {"rom:/other", "Eden"}}};
    choices.apply(games);
    expect(games[0].emulator == "Eden" && games[0].launch.program == "eden",
           "a pick the game no longer lists, or for another game, changes nothing");
}

} // namespace

int main() {
    pickPointsLaunchAtTheEmulator();
    unknownPickIsIgnored();
    std::puts("emulator_choice: ok");
    return 0;
}
