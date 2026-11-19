// A tile's runner mark: a store's or an own game's shipped mark, openSU's runner's, or another
// emulator's own icon over its initials.
#include "runner_mark.hpp"

#include <string>

#include "check.hpp"
#include "support/test_main.hpp"

namespace {

using opensu::library::EmulatorOption;
using opensu::library::Game;
using opensu::library::Source;
using opensu::test::expect;
using opensu::ui::Icon;
using opensu::ui::initials;
using opensu::ui::runnerMark;

void testInitials() {
    expect(initials("DuckStation") == "DS", "a name's capitals");
    expect(initials("Ryujinx") == "Ry", "a one-word name's first two letters");
    expect(initials("Dolphin Emu") == "DE", "a name's words' initials");
    expect(initials("eden") == "Ed", "a lower-case name starts upper-case");
    expect(initials("Snes9x (built-in)") == "Sn", "a note in brackets is no part of the name");
    expect(initials("").empty(), "no name, no letters");
}

Game rom(EmulatorOption option) {
    Game game;
    game.source = Source::Rom;
    game.emulator = option.name;
    game.emulatorOptions.push_back(std::move(option));
    return game;
}

void testMarks() {
    Game steam;
    steam.source = Source::Steam;
    const auto store = runnerMark(steam);
    expect(store && store->icon == Icon::Steam, "a store game carries its store's mark");

    EmulatorOption core;
    core.name = "LRPS2";
    core.builtIn = true;
    const auto own = runnerMark(rom(core));
    expect(own && own->icon == Icon::Libretro, "openSU's runner's own mark");

    EmulatorOption duck;
    duck.name = "DuckStation";
    Game played = rom(duck);
    played.runnerIcon = "/kept/duckstation.png";
    const auto mark = runnerMark(played);
    expect(mark && !mark->icon && mark->picture == played.runnerIcon && mark->letters == "DS",
           "another emulator's icon, over its initials");

    Game unpicked;
    unpicked.source = Source::Rom;
    expect(!runnerMark(unpicked), "a ROM on no emulator carries no mark");

    Game desk;
    desk.source = Source::Remote;
    desk.hostName = "Desk";
    desk.remote.source = "steam";
    const auto there = runnerMark(desk);
    expect(there && there->icon == Icon::Steam && there->remote,
           "another openSU's game carries its store's mark there, ringed");
    desk.remote = {.platform = "PlayStation", .source = "rom", .option = "DuckStation", .home = {}};
    const auto rom = runnerMark(desk);
    expect(rom && rom->letters == "DS" && rom->remote,
           "its ROM, the initials of its emulator there");
    desk.remote = {.platform = "Windows \xC2\xB7 Wine", .source = "own", .option = {}, .home = {}};
    const auto wine = runnerMark(desk);
    expect(wine && wine->icon == Icon::Wine, "its own game, by what it runs as there");
}

void testWhereElse() {
    Game here;
    here.source = Source::Rom;
    expect(opensu::ui::whereElse(here).empty(), "a game only here says nothing");
    here.alsoOn = {"Desk", "Loft"};
    expect(opensu::ui::whereElse(here) == "Also on Desk, Loft", "one here too says where else");
    Game there;
    there.source = Source::Remote;
    there.hostName = "Desk";
    expect(opensu::ui::whereElse(there) == "On Desk", "one only there says where");
}

} // namespace

int main() try {
    testInitials();
    testMarks();
    testWhereElse();
    return 0;
} catch (...) {
    return opensu::test::escaped();
}
