// The mapping editor as the player drives it: a game's page, a button picked and kept on disk, a
// chord heard and given an output, the refusals, Steam Input taking the game, and Settings ›
// Controls.
#include "app/mapping_editor.hpp"

#include <cstdio>
#include <exception>
#include <filesystem>
#include <string>
#include <vector>

#include "support/scratch_dir.hpp"
#include "ui/check.hpp"

namespace {

namespace fs = std::filesystem;
using namespace opensu;
using gamepad::Button;
using test::expect;
using Kind = input::MappingScope::Kind;

struct Said {
    std::string text;
    bool error{false};
};

gamepad::Event event(Button button, bool pressed) {
    return gamepad::Event{gamepad::Event::Kind::Button, button, pressed, {}, {}};
}

library::Game romGame(std::string id) {
    library::Game game;
    game.id = std::move(id);
    game.source = library::Source::Rom;
    game.title = "Kirby";
    game.emulator = "mGBA";
    game.emulatorOptions = {library::EmulatorOption{"mGBA", true, {}, "mgba", false, {}}};
    return game;
}

library::Game steamGame() {
    library::Game game;
    game.id = "steam:620";
    game.title = "Portal 2";
    return game;
}

struct Rig {
    explicit Rig(const fs::path& dir) : file{dir / "mappings.json"} {
    }

    [[nodiscard]] input::MappingScopes onDisk() const {
        return settings::MappingStore{file}.load();
    }
    void focus(const std::string& id) {
        for (std::size_t i = 0; i < page.rows().size(); ++i) {
            if (page.rows()[i].id == id) {
                while (page.focus() != i) {
                    editor.act(page.focus() < i ? Button::Down : Button::Up);
                }
                return;
            }
        }
        expect(false, "a row with that id is on the page");
    }
    [[nodiscard]] const ui::MappingRow& row(const std::string& id) const {
        for (const ui::MappingRow& shown : page.rows()) {
            if (shown.id == id) {
                return shown;
            }
        }
        expect(false, "a row with that id is on the page");
        return page.rows().front();
    }
    /// Picks the list entry labelled `label`.
    void choose(const std::string& label) {
        expect(page.mode() == ui::MappingMode::Picking, "a list is up");
        while (page.choice() > 0) {
            editor.act(Button::Up);
        }
        while (page.choices()[page.choice()] != label) {
            expect(page.choice() + 1 < page.choices().size(), "the list has the entry");
            editor.act(Button::Down);
        }
        editor.act(Button::A);
    }

    fs::path file;
    audio::SdlSoundPlayer sounds;
    input::Shortcuts shortcuts;
    std::vector<Said> said;
    int changes{0};
    ui::MappingPage page;
    app::MappingEditor editor{
        page, settings::MappingStore{file}, sounds, shortcuts,
        app::MappingEditor::Hooks{[this](const std::string& text, bool error) {
                                      said.push_back({text, error});
                                  },
                                  [this] {
                                      ++changes;
                                  },
                                  [] {
                                      return std::vector<app::MappingEditor::Emulator>{
                                          {"pcsx2", "PCSX2"}, {"mgba", "mGBA"}};
                                  }}};
};

void aGameGetsItsOwnMapping(const fs::path& dir) {
    Rig rig{dir};
    const library::Game game = romGame("rom:/r/Kirby.gba");
    expect(app::mappingKeys(game) == input::GameKeys{game.id, "mgba"},
           "a ROM's mapping is also found by its emulator's catalog id");
    rig.editor.openGame(game);
    expect(rig.editor.active() && rig.page.state() == "rows" &&
               rig.page.title() == "Controls · Kirby",
           "the page opens on the game");
    expect(rig.page.rows().front().id == "steam" && rig.row("button.a").value == "A" &&
               rig.page.note().starts_with("Every button sends itself"),
           "with its Steam Input switch, then every button sending itself");

    rig.focus("button.a");
    rig.editor.act(Button::A);
    expect(rig.page.state() == "picking" && rig.page.choices()[rig.page.choice()] == "A",
           "A lists what the button can send, on what it sends now");
    rig.editor.act(Button::B);
    expect(rig.page.state() == "rows" && rig.onDisk() == input::MappingScopes{},
           "B leaves the list without a change");

    rig.editor.act(Button::A);
    rig.choose("B");
    expect(rig.row("button.a").value == "B" && rig.page.state() == "rows",
           "the row reads its new output");
    const input::MappingScopes saved = rig.onDisk();
    const input::ButtonMap* own = saved.at(input::MappingScope{Kind::Game, game.id});
    expect(own != nullptr && own->output(Button::A) == input::PadOutput{Button::B} &&
               rig.changes == 1,
           "the game's own mapping is on disk and the shell told");
    expect(rig.editor.forGame(game).output(Button::A) == input::PadOutput{Button::B} &&
               rig.editor.forGame(romGame("rom:/r/Other.gba")).empty(),
           "it plays for the game alone");

    rig.focus("button.x");
    rig.editor.act(Button::A);
    rig.choose("Select + Start");
    expect(rig.row("button.x").value == "Select + Start", "a button can send two");

    rig.focus("reset");
    rig.editor.act(Button::A);
    expect(rig.editor.forGame(game).empty() && rig.row("button.a").value == "A",
           "reset gives the game what it would inherit");
    rig.editor.act(Button::B);
    expect(!rig.editor.active(), "B closes the page");
}

void anEmulatorsMappingIsSharedAndCopied(const fs::path& dir) {
    Rig rig{dir};
    rig.editor.openEmulator("mgba", "mGBA");
    rig.focus("button.l1");
    rig.editor.act(Button::A);
    rig.choose("R1");
    rig.editor.close();
    const library::Game kirby = romGame("rom:/r/Kirby.gba");
    expect(rig.editor.forGame(kirby).output(Button::L1) == input::PadOutput{Button::R1} &&
               rig.editor.forGame(romGame("rom:/r/Metroid.gba")).output(Button::L1) ==
                   input::PadOutput{Button::R1},
           "every game the emulator runs plays with its mapping");
    expect(rig.editor.forGame(steamGame()).empty(), "a native game does not");

    rig.editor.openGame(kirby);
    expect(rig.row("button.l1").value == "R1" &&
               rig.page.note().starts_with("From its emulator's mapping"),
           "a game shows what it inherits and from where");
    rig.focus("button.a");
    rig.editor.act(Button::A);
    rig.choose("Y");
    expect(rig.editor.forGame(kirby).output(Button::L1) == input::PadOutput{Button::R1} &&
               rig.editor.forGame(kirby).output(Button::A) == input::PadOutput{Button::Y},
           "a change makes the game's own from the one it showed");
    expect(rig.onDisk().at(input::MappingScope{Kind::Emulator, "mgba"})->output(Button::A) ==
               input::PadOutput{Button::A},
           "and leaves the emulator's alone");
}

void aChordIsHeardThenGivenAnOutput(const fs::path& dir) {
    Rig rig{dir};
    const library::Game game = romGame("rom:/r/Kirby.gba");
    rig.editor.openGame(game);
    rig.focus("add");
    rig.editor.act(Button::A);
    expect(rig.editor.capturing() && rig.page.state() == "listening",
           "Add a chord listens for one");

    rig.editor.capturePad({event(Button::Guide, true), event(Button::X, true)});
    expect(rig.editor.capturing() && rig.said.back().error &&
               rig.said.back().text.find("Guide") != std::string::npos,
           "a Guide chord is refused and the page keeps listening");
    rig.editor.capturePad({event(Button::X, false), event(Button::Guide, false)});

    expect(
        rig.shortcuts.rebind(input::Action::SaveState, input::PadChord{Button::Select, Button::R1})
            .empty(),
        "the shell takes Select + R1");
    rig.editor.capturePad({event(Button::Select, true), event(Button::R1, true)});
    expect(rig.editor.capturing() &&
               rig.said.back().text.find("already Save state") != std::string::npos,
           "a chord the shell binds is refused");
    rig.editor.capturePad({event(Button::R1, false), event(Button::Select, false)});

    rig.editor.capturePad({event(Button::Start, true), event(Button::L2, true)});
    expect(!rig.editor.capturing() && rig.page.state() == "picking" &&
               rig.page.pickTitle() == "Start + L2 does",
           "a free chord is heard and its outputs listed");
    rig.choose("Select + Start");
    const input::ButtonMap map = rig.editor.forGame(game);
    expect(map.chords().size() == 1 &&
               map.chords().front().chord == input::PadChord{Button::Start, Button::L2} &&
               map.chords().front().output ==
                   input::ChordOutput{input::PadOutput{Button::Select, Button::Start}},
           "the chord is the game's");
    expect(rig.row("chord.start.l2").value == "Select + Start", "and has its row");

    rig.focus("chord.start.l2");
    rig.editor.act(Button::A);
    rig.choose("Load state");
    expect(rig.editor.forGame(game).chords().front().output ==
               input::ChordOutput{input::Action::LoadState},
           "a chord can perform an action instead");
    rig.editor.act(Button::A);
    rig.choose("Remove this chord");
    expect(rig.editor.forGame(game).chords().empty(), "and can be removed");

    rig.focus("add");
    rig.editor.act(Button::A);
    rig.editor.capturePad({event(Button::B, true), event(Button::B, false)});
    expect(!rig.editor.capturing() && rig.page.state() == "rows", "B alone stops listening");
}

void steamInputRefusesTheMapping(const fs::path& dir) {
    Rig rig{dir};
    const library::Game game = steamGame();
    rig.editor.openGame(game);
    expect(rig.row("steam").value == "Off (every game)" &&
               rig.page.note().find("Steam may read the pad itself") != std::string::npos,
           "a Steam game's page says Steam may read the pad");
    rig.focus("button.a");
    rig.editor.act(Button::A);
    rig.choose("B");
    expect(rig.editor.forGame(game).output(Button::A) == input::PadOutput{Button::B},
           "with Steam Input off the game is mapped");

    rig.focus("steam");
    rig.editor.act(Button::A);
    rig.choose("On");
    expect(rig.row("steam").value == "On" && rig.onDisk().steamInputOf(game.id) == true,
           "Steam Input is turned on for the game and kept");
    expect(rig.editor.forGame(game).empty(), "and no mapping applies to it");
    expect(rig.page.note() ==
               "Steam Input has this game: no mapping applies; openSU's chords still work",
           "the page says why");

    const std::size_t before = rig.said.size();
    rig.focus("button.b");
    rig.editor.act(Button::A);
    expect(rig.page.state() == "rows" && rig.said.size() == before + 1 && rig.said.back().error &&
               rig.said.back().text.find("Steam Input has this game's controls") !=
                   std::string::npos,
           "a button row is refused, saying why");
    rig.focus("add");
    rig.editor.act(Button::A);
    expect(!rig.editor.capturing() && rig.said.size() == before + 2, "so is adding a chord");

    rig.focus("steam");
    rig.editor.act(Button::A);
    rig.choose("Off");
    expect(rig.editor.forGame(game).output(Button::A) == input::PadOutput{Button::B},
           "turned off, the kept mapping applies again");
}

void settingsControlsHoldsTheOtherScopes(const fs::path& dir) {
    Rig rig{dir};
    const std::vector<ui::SettingsRow> rows = rig.editor.settingsRows();
    expect(rows.size() == 3 && rows[0].id == "mapping.global" && rows[0].value == "None" &&
               rows[1].id == "mapping.emulator" && rows[1].value == "PCSX2" &&
               rows[2].id == "mapping.steamInput",
           "the global mapping, an emulator's and the Steam Input switch");
    expect(app::MappingEditor::owns("mapping.emulator") && !app::MappingEditor::owns("ui.scale"),
           "the editor owns its rows");

    rig.editor.runSetting("mapping.emulator", 1);
    expect(rig.editor.settingsRows()[1].value == "mGBA", "Right picks the next emulator");
    rig.editor.runSetting("mapping.emulator", 0);
    expect(rig.editor.active() && rig.page.title() == "Controls · mGBA" &&
               rig.page.rows().front().id == "button.a",
           "A opens its mapping, which has no Steam Input row");
    rig.editor.close();

    rig.editor.runSetting("mapping.global", 0);
    expect(rig.page.title() == "Controls · every game", "the global mapping opens");
    rig.focus("button.b");
    rig.editor.act(Button::A);
    rig.choose("A");
    rig.editor.close();
    expect(rig.editor.settingsRows()[0].value == "Custom" &&
               rig.editor.forGame(steamGame()).output(Button::B) == input::PadOutput{Button::A},
           "it applies to every game");

    rig.editor.runSetting("mapping.steamInput", 0);
    expect(rig.onDisk().steamInputGlobal() && rig.editor.forGame(steamGame()).empty() &&
               rig.editor.forGame(romGame("rom:/r/Kirby.gba")).empty(),
           "Steam Input on for every game drops every mapping");
}

} // namespace

int main() {
    try {
        const test::ScratchDir scratch{OPENSU_TEST_SCRATCH, "mapping_editor"};
        aGameGetsItsOwnMapping(scratch.path() / "game");
        anEmulatorsMappingIsSharedAndCopied(scratch.path() / "emulator");
        aChordIsHeardThenGivenAnOutput(scratch.path() / "chord");
        steamInputRefusesTheMapping(scratch.path() / "steam");
        settingsControlsHoldsTheOtherScopes(scratch.path() / "settings");
        std::printf("mapping_editor: all checks passed\n");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAIL: unhandled exception: %s\n", error.what());
        return 1;
    }
}
