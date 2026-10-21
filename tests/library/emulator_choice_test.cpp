// The emulator picked for a ROM, kept at the game and system setting layers: it points the game's
// launch at the pick, a pick that is not installed says so, and each pick names where it is from.
#include "library/emulator_choice.hpp"

#include <cstdio>
#include <vector>

#include "ui/check.hpp"

namespace {

using namespace opensu::library;
using opensu::settings::Layer;
using opensu::settings::LayerKeys;
using opensu::settings::SettingLayers;
using opensu::test::expect;

Game kirby() {
    Game game;
    game.id = "rom:/r/Kirby.nsp";
    game.source = Source::Rom;
    game.sourceId = "switch";
    game.installed = true;
    game.emulatorOptions = {
        EmulatorOption{"Eden", true, LaunchSpec{"eden", {"/r/Kirby.nsp"}}, "eden"},
        EmulatorOption{"Ryujinx", true, LaunchSpec{"ryujinx", {"/r/Kirby.nsp"}}, "ryujinx"},
        EmulatorOption{"Yuzu", false, {}},
    };
    game.emulator = "Eden";
    game.launch = game.emulatorOptions.front().launch;
    return game;
}

void pickPointsLaunchAtTheEmulator() {
    std::vector<Game> games{kirby()};
    SettingLayers layers;
    const Layer own{Layer::Kind::Game, games[0].id};
    applyEmulatorPicks(layers, games);
    expect(games[0].emulator == "Eden" &&
               pickFor(layers, games[0]).source == EmulatorSource::Search,
           "a game with no pick keeps the search's emulator");

    layers.set(Layer{Layer::Kind::System, "switch"}, emulatorKey, "Ryujinx");
    applyEmulatorPicks(layers, games);
    expect(games[0].emulator == "Ryujinx" &&
               pickFor(layers, games[0]).source == EmulatorSource::System,
           "the system's pick runs a game without its own");

    layers.set(own, emulatorKey, "Eden");
    applyEmulatorPicks(layers, games);
    expect(games[0].emulator == "Eden" && games[0].launch.program == "eden" &&
               pickFor(layers, games[0]).source == EmulatorSource::Game,
           "the game's own pick comes before its system's");

    layers.set(own, emulatorKey, "Yuzu");
    applyEmulatorPicks(layers, games);
    expect(games[0].launch.empty() && games[0].unavailable == "Yuzu is not installed",
           "a pick that is not installed cannot launch and says so");

    layers.set(own, emulatorKey, "Ryujinx");
    applyEmulatorPicks(layers, games);
    expect(games[0].launch.program == "ryujinx" && games[0].unavailable.empty(),
           "picking an installed emulator again launches");
    expect(layerKeysOf(games[0]) == LayerKeys{games[0].id, "switch", "ryujinx"},
           "its settings are then read through that emulator's layer");
}

void unknownPickIsIgnored() {
    std::vector<Game> games{kirby()};
    SettingLayers layers;
    layers.set(Layer{Layer::Kind::Game, games[0].id}, emulatorKey, "Gone");
    layers.set(Layer{Layer::Kind::Game, "rom:/other"}, emulatorKey, "Ryujinx");
    layers.set(Layer{Layer::Kind::System, "switch"}, emulatorKey, "Ryujinx");
    applyEmulatorPicks(layers, games);
    expect(games[0].emulator == "Eden" && games[0].launch.program == "eden",
           "a pick the game no longer lists keeps the search's, the system's pick included; one "
           "for another game changes nothing");
}

} // namespace

int main() {
    pickPointsLaunchAtTheEmulator();
    unknownPickIsIgnored();
    std::puts("emulator_choice: ok");
    return 0;
}
