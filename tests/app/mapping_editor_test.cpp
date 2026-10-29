// The mapping editor as the player drives it: a game's page, a button picked and kept on disk, a
// chord heard and given an output, the refusals, Steam Input taking the game, and Settings ›
// Controls.
#include "app/mapping_editor.hpp"

#include <array>
#include <cstdio>
#include <deque>
#include <exception>
#include <filesystem>
#include <functional>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "support/scratch_dir.hpp"
#include "support/test_main.hpp"
#include "ui/check.hpp"

namespace {

namespace fs = std::filesystem;
using namespace opensu;
using gamepad::Button;
using test::expect;
using Kind = settings::Layer::Kind;

struct Said {
    std::string text;
    bool error{false};
};

gamepad::Event event(Button button, bool pressed) {
    gamepad::Event event;
    event.button = button;
    event.pressed = pressed;
    return event;
}

constexpr std::array<std::pair<ui::Direction, Button>, 4> moves{
    {{ui::Direction::Up, Button::Up},
     {ui::Direction::Down, Button::Down},
     {ui::Direction::Left, Button::Left},
     {ui::Direction::Right, Button::Right}}};

/// The direction buttons that take `start`'s focus (its choice while picking) to where `arrived`
/// holds, found on copies of the page; nothing when no path gets there.
std::optional<std::vector<Button>>
route(const ui::MappingPage& start, const std::function<bool(const ui::MappingPage&)>& arrived) {
    const bool picking = start.mode() == ui::MappingMode::Picking;
    const auto at = [picking](const ui::MappingPage& page) {
        return picking ? page.choice() : page.focus();
    };
    std::deque<std::pair<ui::MappingPage, std::vector<Button>>> queue{{start, {}}};
    std::set<std::size_t> seen{at(start)};
    while (!queue.empty()) {
        auto [page, path] = std::move(queue.front());
        queue.pop_front();
        if (arrived(page)) {
            return path;
        }
        for (const auto& [direction, button] : moves) {
            ui::MappingPage next = page;
            if (next.move(direction) && seen.insert(at(next)).second) {
                std::vector<Button> longer = path;
                longer.push_back(button);
                queue.emplace_back(std::move(next), std::move(longer));
            }
        }
    }
    return std::nullopt;
}

/// How many items, or choices while picking, `page`'s focus can reach from where it is.
std::size_t reachable(const ui::MappingPage& page) {
    const bool picking = page.mode() == ui::MappingMode::Picking;
    const std::size_t count = picking ? page.picker().choices.size() : page.items().size();
    std::size_t reached = 0;
    for (std::size_t i = 0; i < count; ++i) {
        reached += route(page,
                         [&](const ui::MappingPage& there) {
                             return (picking ? there.choice() : there.focus()) == i;
                         })
                       ? 1
                       : 0;
    }
    return reached;
}

library::Game romGame(std::string id) {
    library::Game game;
    game.id = std::move(id);
    game.source = library::Source::Rom;
    game.sourceId = "gba";
    game.title = "Kirby";
    game.emulator = "mGBA";
    game.emulatorOptions = {library::EmulatorOption{"mGBA", true, {}, "mgba", false, {}, {}, {}}};
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
    /// Walks focus to the item `id` with the direction buttons.
    void focus(const std::string& id) {
        const std::optional<std::vector<Button>> path =
            route(page, [&id](const ui::MappingPage& there) {
                return there.focused() != nullptr && there.focused()->id == id;
            });
        expect(path.has_value(), "the item is on the page and focus reaches it");
        for (const Button button : path.value_or(std::vector<Button>{})) {
            editor.act(button);
        }
        expect(page.focusName() == id, "focus walked there");
    }
    [[nodiscard]] const ui::MappingItem& row(const std::string& id) const {
        for (const ui::MappingItem& shown : page.items()) {
            if (shown.id == id) {
                return shown;
            }
        }
        expect(false, "an item with that id is on the page");
        return page.items().front();
    }
    [[nodiscard]] std::string current() const {
        return page.picker().choices.at(page.choice()).label;
    }
    /// Walks the picker to the tile labelled `label` and picks it.
    void choose(const std::string& label) {
        expect(page.mode() == ui::MappingMode::Picking, "the picker is up");
        const std::optional<std::vector<Button>> path =
            route(page, [&label](const ui::MappingPage& there) {
                return there.focusName() == label;
            });
        expect(path.has_value(), "the picker has the tile and focus reaches it");
        for (const Button button : path.value_or(std::vector<Button>{})) {
            editor.act(button);
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
    expect(library::layerKeysOf(game) == settings::LayerKeys{game.id, "gba", "mgba"},
           "a ROM's mapping is also found by its system and its emulator's catalog id");
    rig.editor.openGame(game);
    expect(rig.editor.active() && rig.page.state() == "board" &&
               rig.page.title() == "Controls · Kirby" && rig.page.focusName() == "button.a",
           "the page opens on the game, focused on A");
    expect(rig.page.items().front().id == "steam" && rig.row("button.a").value == "A" &&
               !rig.row("button.a").changed &&
               rig.page.note().starts_with("Every button sends itself"),
           "with its Steam Input switch, then every button sending itself");
    std::vector<std::string> ids;
    for (const ui::MappingItem& item : rig.page.items()) {
        ids.push_back(item.id);
    }
    expect(ids == std::vector<std::string>{"steam", "button.a", "button.b", "button.x", "button.y",
                                           "button.l1", "button.r1", "button.l2", "button.r2",
                                           "button.l3", "button.r3", "button.select",
                                           "button.start", "button.up", "button.down",
                                           "button.left", "button.right", "add"},
           "the items keep the control channel's ids");
    expect(reachable(rig.page) == rig.page.items().size(), "focus reaches every item");

    rig.editor.act(Button::A);
    expect(rig.page.state() == "picking" && rig.current() == "A" &&
               rig.page.picker().glyphs == std::vector<std::string>{"A"},
           "A puts up what the button can send, headed by its glyph, on what it sends now");
    expect(rig.page.picker().choices.size() == 18 && reachable(rig.page) == 18,
           "a tile per pad button, Nothing and Select + Start, every one reachable");
    rig.editor.act(Button::B);
    expect(rig.page.state() == "board" && rig.onDisk() == input::MappingScopes{},
           "B closes the picker without a change");

    rig.editor.act(Button::A);
    rig.choose("B");
    expect(rig.row("button.a").value == "B" && rig.row("button.a").changed &&
               rig.row("button.a").valueGlyphs == std::vector<std::string>{"B"} &&
               rig.page.state() == "board",
           "the chip reads its new output, as a glyph");
    const input::MappingScopes saved = rig.onDisk();
    const input::ButtonMap* own = saved.at(settings::Layer{Kind::Game, game.id});
    expect(own != nullptr && own->output(Button::A) == input::PadOutput{Button::B} &&
               rig.changes == 1,
           "the game's own mapping is on disk and the shell told");
    expect(rig.editor.forGame(game).output(Button::A) == input::PadOutput{Button::B} &&
               rig.editor.forGame(romGame("rom:/r/Other.gba")).empty(),
           "it plays for the game alone");

    rig.focus("button.x");
    rig.editor.act(Button::A);
    rig.choose("Select + Start");
    expect(rig.row("button.x").value == "Select + Start" &&
               rig.row("button.x").valueGlyphs == std::vector<std::string>{"-", "+"},
           "a button can send two");
    rig.focus("button.y");
    rig.editor.act(Button::A);
    rig.choose("Nothing");
    expect(rig.row("button.y").value == "Nothing" && rig.row("button.y").valueGlyphs.empty(),
           "or nothing");

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
    expect(rig.onDisk().at(settings::Layer{Kind::Emulator, "mgba"})->output(Button::A) ==
               input::PadOutput{Button::A},
           "and leaves the emulator's alone");
}

void aChordIsHeardThenGivenAnOutput(const fs::path& dir) {
    Rig rig{dir};
    const library::Game game = romGame("rom:/r/Kirby.gba");
    rig.editor.openGame(game);
    rig.focus("add");
    rig.editor.act(Button::A);
    expect(rig.editor.capturing() && rig.page.state() == "listening" &&
               rig.page.prompt() == "Hold a button, then press another",
           "Add chord listens for one");

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
               rig.page.picker().glyphs == std::vector<std::string>{"+", "LT"},
           "a free chord is heard and its outputs put up, headed by its glyphs");
    expect(rig.current() == "Quick menu" && reachable(rig.page) == rig.page.picker().choices.size(),
           "the actions first, every tile reachable");
    rig.choose("Select + Start");
    const input::ButtonMap map = rig.editor.forGame(game);
    expect(map.chords().size() == 1 &&
               map.chords().front().chord == input::PadChord{Button::Start, Button::L2} &&
               map.chords().front().output ==
                   input::ChordOutput{input::PadOutput{Button::Select, Button::Start}},
           "the chord is the game's");
    expect(rig.row("chord.start.l2").value == "Select + Start" &&
               rig.row("chord.start.l2").glyphs == std::vector<std::string>{"+", "LT"},
           "and has its chip");
    expect(reachable(rig.page) == rig.page.items().size(), "which focus reaches");

    rig.focus("chord.start.l2");
    rig.editor.act(Button::A);
    rig.choose("Load state");
    expect(rig.editor.forGame(game).chords().front().output ==
               input::ChordOutput{input::Action::LoadState},
           "a chord can perform an action instead");
    rig.editor.act(Button::A);
    expect(rig.current() == "Load state" && rig.page.picker().choices.back().label == "Remove",
           "the picker opens on what the chord does, with Remove");
    rig.choose("Remove");
    expect(rig.editor.forGame(game).chords().empty(), "and can be removed");

    rig.focus("add");
    rig.editor.act(Button::A);
    rig.editor.capturePad({event(Button::B, true), event(Button::B, false)});
    expect(!rig.editor.capturing() && rig.page.state() == "board", "B alone stops listening");
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
    expect(rig.page.picker().label == "Steam Input" && rig.page.picker().choices.size() == 3 &&
               rig.current() == "As every game (Off)",
           "Steam Input puts up its three settings, on the one in force");
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
    expect(rig.page.state() == "board" && rig.said.size() == before + 1 && rig.said.back().error &&
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
               rows[1].id == "mapping.emulators" && rows[1].kind == ui::RowKind::Page &&
               rows[1].value == "2" && rows[2].id == "mapping.steamInput",
           "the global mapping, the page of emulators' and the Steam Input switch");
    expect(app::MappingEditor::owns("mapping.emulators") &&
               app::MappingEditor::owns("mapping.emulator.mgba") &&
               !app::MappingEditor::owns("ui.scale"),
           "the editor owns its rows");
    expect(!rig.editor.settingsPage("mapping.global"), "only the emulators' row opens a page");
    const ui::SettingsCategory page =
        test::need(rig.editor.settingsPage("mapping.emulators"), "the emulators' page");
    expect(page.rows.size() == 2 && page.rows[0].id == "mapping.emulator.pcsx2" &&
               page.rows[1].id == "mapping.emulator.mgba" &&
               page.rows[1].kind == ui::RowKind::Action && page.rows[1].value == "None",
           "the page lists one row per emulator");

    rig.editor.runSetting("mapping.emulator.mgba", 1);
    expect(!rig.editor.active(), "Left and Right do nothing on an emulator's row");
    rig.editor.runSetting("mapping.emulator.mgba", 0);
    expect(rig.editor.active() && rig.page.title() == "Controls · mGBA" &&
               rig.page.items().front().id == "button.a",
           "A opens its mapping, which has no Steam Input switch");
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

int main() try {
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
} catch (...) {
    return opensu::test::escaped();
}
