// Button mappings: what a mapping takes, what the pads are given for it, and which mapping applies
// to a game: its own, else its emulator's, else the global one, none while Steam Input has it.
#include "input/button_map.hpp"

#include <cstdio>

#include "check.hpp"
#include "input/mapping_scopes.hpp"

namespace {

using namespace opensu::input;
using opensu::gamepad::Button;
using opensu::gamepad::Chord;
using opensu::test::expect;
using Kind = MappingScope::Kind;

ButtonMap sending(Button from, PadOutput to) {
    ButtonMap map;
    expect(map.setButton(from, std::move(to)).empty(), "the button takes the output");
    return map;
}

void aMapTakesPadButtonsOnly() {
    ButtonMap map;
    expect(map.empty() && map.output(Button::A) == PadOutput{Button::A},
           "a button with nothing set sends itself");
    expect(map.setButton(Button::A, {Button::B}).empty() &&
               map.output(Button::A) == PadOutput{Button::B},
           "A can send B");
    expect(map.setButton(Button::X, {}).empty() && map.output(Button::X).empty(),
           "X can send nothing");
    expect(map.setButton(Button::A, {Button::A}).empty() && !map.buttons().contains(Button::A),
           "sending itself again leaves the map");
    expect(!map.setButton(Button::Guide, {Button::A}).empty(), "Guide is openSU's");
    expect(!map.setButton(Button::A, {Button::Guide}).empty(), "and is never sent to a game");
    expect(!map.setChord(PadChord{Button::Guide, Button::X}, PadOutput{Button::A}).empty(),
           "Guide's chords are openSU's");
    expect(!map.setChord(PadChord{Button::L1, Button::L1}, PadOutput{Button::A}).empty(),
           "a chord needs two buttons");
    expect(!map.setChord(PadChord{Button::L1, Button::R1}, Action::Confirm).empty(),
           "a chord performs only an action a chord can");
    expect(
        map.setChord(PadChord{Button::L1, Button::R1}, Action::SaveState).empty() &&
            map.setChord(PadChord{Button::L1, Button::R1}, PadOutput{Button::Select, Button::Start})
                .empty() &&
            map.chords().size() == 1,
        "setting a chord again replaces what it does");
    expect(describe(map.chords().front().output) == "Select + Start" &&
               describe(PadOutput{}) == "Nothing" &&
               describe(ChordOutput{Action::LoadState}) == "Load state",
           "outputs have names");
    map.removeChord(PadChord{Button::L1, Button::R1});
    expect(map.chords().empty(), "a chord can be removed");
}

void thePadsGetButtonsAndPadChords() {
    Shortcuts shell;
    ButtonMap map = sending(Button::A, {Button::B});
    expect(map.setChord(PadChord{Button::Select, Button::R1}, Action::SaveState).empty() &&
               map.setChord(PadChord{Button::Select, Button::L1}, PadOutput{Button::Start}).empty(),
           "an action chord and a button chord");
    const opensu::gamepad::ForwardMap forward = forwardMap(map, shell);
    expect(forward.outputOf(Button::A) == std::set{Button::B} &&
               forward.outputOf(Button::Y) == std::set{Button::Y},
           "the pads send what the map says and the rest as it is");
    expect(forward.chords.size() == 1 &&
               forward.chords.at(Chord{Button::Select, Button::L1}) == std::set{Button::Start},
           "only a chord that sends buttons is the pads' to play");
    expect(shell.rebind(Action::Devices, PadChord{Button::Select, Button::L1}).empty() &&
               forwardMap(map, shell).chords.empty(),
           "a chord the shell binds stays the shell's");
}

void theNearestScopeApplies() {
    MappingScopes scopes;
    const GameKeys rom{"rom:/r/Kirby.gba", "mgba"};
    const GameKeys other{"rom:/r/Metroid.gba", "mgba"};
    const GameKeys native{"steam:620", ""};
    expect(scopes.resolve(rom).empty() && !scopes.source(rom),
           "with nothing kept, nothing applies");

    scopes.set(MappingScope{}, sending(Button::A, {Button::B}));
    expect(scopes.resolve(native).output(Button::A) == PadOutput{Button::B} &&
               scopes.resolve(rom).output(Button::A) == PadOutput{Button::B},
           "the global mapping applies to native and emulated games alike");

    scopes.set(MappingScope{Kind::Emulator, "mgba"}, sending(Button::X, {Button::Y}));
    expect(scopes.resolve(rom).output(Button::X) == PadOutput{Button::Y} &&
               scopes.resolve(other).output(Button::X) == PadOutput{Button::Y},
           "every game an emulator runs shares its mapping");
    expect(scopes.resolve(rom).output(Button::A) == PadOutput{Button::A},
           "which replaces the global one whole");
    expect(scopes.resolve(native).output(Button::A) == PadOutput{Button::B},
           "a native game keeps the global one");

    scopes.set(MappingScope{Kind::Game, rom.game}, sending(Button::L1, {Button::R1}));
    expect(scopes.resolve(rom).output(Button::L1) == PadOutput{Button::R1} &&
               scopes.resolve(rom).output(Button::X) == PadOutput{Button::X},
           "a game's own mapping replaces its emulator's whole");
    expect(scopes.source(rom) == MappingScope{Kind::Game, rom.game} &&
               scopes.source(other) == MappingScope{Kind::Emulator, "mgba"} &&
               scopes.source(native) == MappingScope{},
           "the source is the nearest scope that keeps one");
    expect(scopes.resolve(other).output(Button::L1) == PadOutput{Button::L1},
           "the other game of the emulator is not touched");

    scopes.clear(MappingScope{Kind::Game, rom.game});
    expect(scopes.resolve(rom).output(Button::X) == PadOutput{Button::Y},
           "clearing the game's mapping gives it its emulator's again");
}

void steamInputTakesTheGame() {
    MappingScopes scopes;
    const GameKeys game{"steam:620", ""};
    scopes.set(MappingScope{}, sending(Button::A, {Button::B}));
    scopes.set(MappingScope{Kind::Game, game.game}, sending(Button::X, {Button::Y}));
    expect(!scopes.steamInput(game.game), "Steam Input is off unless chosen");
    scopes.setSteamInputGlobal(true);
    expect(scopes.steamInput(game.game) && scopes.resolve(game).empty(),
           "on for every game, no mapping applies, the game's own included");
    scopes.setSteamInput(game.game, false);
    expect(!scopes.steamInput(game.game) &&
               scopes.resolve(game).output(Button::X) == PadOutput{Button::Y},
           "a game can turn it off for itself");
    scopes.setSteamInputGlobal(false);
    scopes.setSteamInput(game.game, true);
    expect(scopes.resolve(game).empty() && !scopes.steamInput("steam:400"),
           "or on for itself alone");
    scopes.setSteamInput(game.game, std::nullopt);
    expect(!scopes.steamInputOf(game.game) && !scopes.steamInput(game.game),
           "and back to the global choice");
}

} // namespace

int main() {
    aMapTakesPadButtonsOnly();
    thePadsGetButtonsAndPadChords();
    theNearestScopeApplies();
    steamInputTakesTheGame();
    std::printf("button_map: all checks passed\n");
    return 0;
}
