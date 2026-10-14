#include "mapping_editor.hpp"

#include <algorithm>
#include <array>
#include <type_traits>
#include <utility>

#include "lucent/log.h"

namespace opensu::app {
namespace {

using gamepad::Button;
using Kind = input::MappingScope::Kind;

constexpr std::string_view steamRow = "steam";
constexpr std::string_view buttonRow = "button.";
constexpr std::string_view chordRow = "chord.";
constexpr std::string_view addRow = "add";
constexpr std::string_view resetRow = "reset";
constexpr std::string_view globalSetting = "mapping.global";
constexpr std::string_view emulatorSetting = "mapping.emulator";
constexpr std::string_view steamSetting = "mapping.steamInput";
constexpr std::string_view listeningPrompt = "Hold a button, then press another";
constexpr std::string_view steamRefusal =
    "Steam Input has this game's controls; turn it off here to map its buttons";

std::string chordId(const input::PadChord& chord) {
    return std::string{chordRow} + std::string{gamepad::name(chord.modifier)} + "." +
           std::string{gamepad::name(chord.trigger)};
}

std::optional<input::PadChord> chordOfId(std::string_view id) {
    if (!id.starts_with(chordRow)) {
        return std::nullopt;
    }
    const std::string_view pair = id.substr(chordRow.size());
    const std::size_t dot = pair.find('.');
    if (dot == std::string_view::npos) {
        return std::nullopt;
    }
    return input::PadChord{gamepad::buttonNamed(pair.substr(0, dot)),
                           gamepad::buttonNamed(pair.substr(dot + 1))};
}

std::string onOff(bool on) {
    return on ? "On" : "Off";
}

/// The pad buttons as the picker groups their tiles: face, shoulders, d-pad, sticks, Select and
/// Start.
constexpr std::array<std::pair<Button, std::size_t>, 16> buttonTiles{{
    {Button::A, 0},
    {Button::B, 0},
    {Button::X, 0},
    {Button::Y, 0},
    {Button::L2, 1},
    {Button::L1, 1},
    {Button::R1, 1},
    {Button::R2, 1},
    {Button::Up, 2},
    {Button::Down, 2},
    {Button::Left, 2},
    {Button::Right, 2},
    {Button::L3, 3},
    {Button::R3, 3},
    {Button::Select, 4},
    {Button::Start, 4},
}};
constexpr std::size_t buttonGroups = 5;

/// The glyph keys of `output`'s buttons, Select and Start first as a chord's modifier would be.
std::vector<std::string> glyphsOf(const input::PadOutput& output) {
    std::vector<Button> buttons{output.begin(), output.end()};
    std::ranges::stable_partition(buttons, [](Button button) {
        return button == Button::Select || button == Button::Start;
    });
    std::vector<std::string> glyphs;
    glyphs.reserve(buttons.size());
    for (const Button button : buttons) {
        glyphs.emplace_back(input::glyphOf(button));
    }
    return glyphs;
}

} // namespace

input::GameKeys mappingKeys(const library::Game& game) {
    input::GameKeys keys{game.id, {}};
    const auto runs =
        std::ranges::find(game.emulatorOptions, game.emulator, &library::EmulatorOption::name);
    if (!game.emulator.empty() && runs != game.emulatorOptions.end()) {
        keys.emulator = runs->id;
    }
    return keys;
}

MappingEditor::MappingEditor(ui::MappingPage& page, settings::MappingStore store,
                             audio::SdlSoundPlayer& sounds, const input::Shortcuts& shell,
                             Hooks hooks)
    : page_{page}, store_{std::move(store)}, sounds_{sounds}, shell_{shell},
      hooks_{std::move(hooks)}, scopes_{store_.load()} {
}

void MappingEditor::openGame(const library::Game& game) {
    open(Target{input::MappingScope{Kind::Game, game.id}, "Controls · " + game.title,
                mappingKeys(game), game.source == library::Source::Steam});
}

void MappingEditor::openEmulator(const std::string& id, const std::string& name) {
    open(Target{input::MappingScope{Kind::Emulator, id}, "Controls · " + name, {}, false});
}

void MappingEditor::openGlobal() {
    open(Target{input::MappingScope{}, "Controls · every game", {}, false});
}

void MappingEditor::open(Target target) {
    target_ = std::move(target);
    picking_ = std::monostate{};
    pickingSteam_ = false;
    // input-sound.md 3.4 Open: a panel appears.
    sounds_.play(audio::Effect::Open);
    page_.open(target_.title, items(), note());
}

void MappingEditor::close() {
    if (!page_.isOpen()) {
        return;
    }
    // input-sound.md 3.4 Close: the player dismisses a panel.
    sounds_.play(audio::Effect::Close);
    page_.close();
    target_ = {};
}

bool MappingEditor::steamInputHasIt() const {
    return page_.isOpen() && target_.scope.kind == Kind::Game &&
           scopes_.steamInput(target_.scope.id);
}

input::ButtonMap MappingEditor::shown() const {
    if (const input::ButtonMap* own = scopes_.at(target_.scope)) {
        return *own;
    }
    if (target_.scope.kind == Kind::Global) {
        return {};
    }
    const input::GameKeys under{
        {}, target_.scope.kind == Kind::Game ? target_.keys.emulator : std::string{}};
    const std::optional<input::MappingScope> from = scopes_.source(under);
    return from ? *scopes_.at(*from) : input::ButtonMap{};
}

std::string MappingEditor::note() const {
    if (steamInputHasIt()) {
        return "Steam Input has this game: no mapping applies; openSU's chords still work";
    }
    const Kind kind = target_.scope.kind;
    std::string text;
    if (scopes_.at(target_.scope) != nullptr) {
        text = kind == Kind::Game       ? "This game's own mapping"
               : kind == Kind::Emulator ? "Shared by every game this emulator runs"
                                        : "Every game's, unless its emulator or the game has one";
    } else {
        const input::GameKeys under{{}, kind == Kind::Game ? target_.keys.emulator : ""};
        const std::optional<input::MappingScope> from = scopes_.source(under);
        text = !from                          ? "Every button sends itself"
               : from->kind == Kind::Emulator ? "From its emulator's mapping"
                                              : "From the mapping for every game";
        text += "; a change makes ";
        text += kind == Kind::Game ? "this game's own" : "its own";
    }
    if (target_.steamGame) {
        text += " · Steam may read the pad itself";
    }
    return text;
}

std::vector<ui::MappingItem> MappingEditor::items() const {
    std::vector<ui::MappingItem> items;
    if (target_.scope.kind == Kind::Game) {
        const std::optional<bool> own = scopes_.steamInputOf(target_.scope.id);
        ui::MappingItem steam;
        steam.id = steamRow;
        steam.trailing = true;
        steam.label = "Steam Input";
        steam.value = own ? onOff(*own) : onOff(scopes_.steamInputGlobal()) + " (every game)";
        items.push_back(std::move(steam));
    }
    const input::ButtonMap map = shown();
    for (const Button button : gamepad::padButtons) {
        const input::PadOutput output = map.output(button);
        items.push_back(ui::MappingItem{std::string{buttonRow} + std::string{gamepad::name(button)},
                                        button,
                                        false,
                                        {std::string{input::glyphOf(button)}},
                                        input::buttonLabel(button),
                                        input::describe(output),
                                        glyphsOf(output),
                                        output != input::PadOutput{button}});
    }
    for (const input::ChordMapping& mapping : map.chords()) {
        const auto* buttons = std::get_if<input::PadOutput>(&mapping.output);
        items.push_back(
            ui::MappingItem{chordId(mapping.chord),
                            Button::None,
                            false,
                            {std::string{input::glyphOf(mapping.chord.modifier)},
                             std::string{input::glyphOf(mapping.chord.trigger)}},
                            input::describe(mapping.chord),
                            input::describe(mapping.output),
                            buttons != nullptr ? glyphsOf(*buttons) : std::vector<std::string>{},
                            true});
    }
    ui::MappingItem add;
    add.id = addRow;
    add.label = "+ Add chord";
    items.push_back(std::move(add));
    if (scopes_.at(target_.scope) != nullptr) {
        ui::MappingItem reset;
        reset.id = resetRow;
        reset.trailing = true;
        reset.label = target_.scope.kind == Kind::Global ? "Clear the mapping"
                                                         : "Use the mapping it would inherit";
        items.push_back(std::move(reset));
    }
    return items;
}

void MappingEditor::show() {
    page_.refresh(items(), note());
}

void MappingEditor::act(Button button) {
    if (!page_.isOpen() || page_.mode() == ui::MappingMode::Listening) {
        return;
    }
    const bool picking = page_.mode() == ui::MappingMode::Picking;
    switch (button) {
    case Button::Up:
    case Button::Down:
    case Button::Left:
    case Button::Right:
        // input-sound.md 3.4 Navigation: a focus move.
        if (page_.move(button == Button::Up     ? ui::Direction::Up
                       : button == Button::Down ? ui::Direction::Down
                       : button == Button::Left ? ui::Direction::Left
                                                : ui::Direction::Right)) {
            sounds_.play(audio::Effect::Navigation);
        }
        break;
    case Button::A:
        if (picking) {
            choose();
        } else if (const ui::MappingItem* item = page_.focused()) {
            runItem(*item);
        }
        break;
    case Button::B:
        if (picking) {
            picking_ = std::monostate{};
            pickingSteam_ = false;
            choices_.clear();
            page_.showBoard(note());
        } else {
            close();
        }
        break;
    default:
        break;
    }
}

void MappingEditor::runItem(const ui::MappingItem& item) {
    if (item.id == steamRow) {
        pickingSteam_ = true;
        const std::optional<bool> own = scopes_.steamInputOf(target_.scope.id);
        page_.pick(ui::MappingPick{
            {},
            "Steam Input",
            {ui::MappingChoice{
                 {}, "As every game (" + onOff(scopes_.steamInputGlobal()) + ")", 0, true},
             ui::MappingChoice{{}, "On"}, ui::MappingChoice{{}, "Off"}},
            !own   ? 0U
            : *own ? 1U
                   : 2U});
        return;
    }
    if (item.id == resetRow) {
        scopes_.clear(target_.scope);
        save();
        show();
        return;
    }
    if (steamInputHasIt()) {
        hooks_.say(std::string{steamRefusal}, true);
        return;
    }
    if (item.id == addRow) {
        capture_.begin();
        page_.listen(std::string{listeningPrompt});
    } else if (item.id.starts_with(buttonRow)) {
        pickButton(gamepad::buttonNamed(std::string_view{item.id}.substr(buttonRow.size())));
    } else if (const std::optional<input::PadChord> chord = chordOfId(item.id)) {
        pickChord(*chord);
    }
}

void MappingEditor::pickButton(Button button) {
    std::vector<Tile> tiles;
    tiles.reserve(buttonTiles.size() + 2);
    for (const auto& [out, group] : buttonTiles) {
        tiles.push_back(Tile{input::PadOutput{out}, group});
    }
    tiles.push_back(Tile{input::PadOutput{}, buttonGroups});
    tiles.push_back(Tile{input::PadOutput{Button::Select, Button::Start}, buttonGroups});
    picking_ = button;
    pick({std::string{input::glyphOf(button)}}, tiles, Choice{shown().output(button)});
}

void MappingEditor::pickChord(const input::PadChord& chord) {
    std::vector<Tile> tiles;
    for (const input::Action action : input::allActions) {
        if (input::takesPadChord(action)) {
            tiles.push_back(Tile{action, 0});
        }
    }
    for (const auto& [out, group] : buttonTiles) {
        tiles.push_back(Tile{input::PadOutput{out}, 1 + group});
    }
    for (const auto& [out, group] : buttonTiles) {
        if (out != Button::Select) {
            tiles.push_back(Tile{input::PadOutput{Button::Select, out}, 1 + buttonGroups});
        }
    }
    std::optional<Choice> current;
    const input::ButtonMap map = shown();
    if (const input::ChordOutput* does = map.chord(chord)) {
        current = std::visit(
            [](const auto& value) {
                return Choice{value};
            },
            *does);
        tiles.push_back(Tile{Remove{}, 2 + buttonGroups});
    }
    picking_ = chord;
    pick({std::string{input::glyphOf(chord.modifier)}, std::string{input::glyphOf(chord.trigger)}},
         tiles, current);
}

void MappingEditor::pick(std::vector<std::string> source, const std::vector<Tile>& tiles,
                         const std::optional<Choice>& current) {
    ui::MappingPick offered{std::move(source), {}, {}, 0};
    choices_.clear();
    for (const Tile& tile : tiles) {
        if (current && tile.choice == *current) {
            offered.current = choices_.size();
        }
        choices_.push_back(tile.choice);
        offered.choices.push_back(std::visit(
            [&tile](const auto& value) {
                using Value = std::decay_t<decltype(value)>;
                if constexpr (std::is_same_v<Value, Remove>) {
                    return ui::MappingChoice{{}, "Remove", tile.group};
                } else if constexpr (std::is_same_v<Value, input::Action>) {
                    return ui::MappingChoice{{}, std::string{input::label(value)}, tile.group};
                } else {
                    return ui::MappingChoice{glyphsOf(value), input::describe(value), tile.group};
                }
            },
            tile.choice));
    }
    page_.pick(std::move(offered));
}

void MappingEditor::choose() {
    sounds_.play(audio::Effect::Navigation);
    const std::size_t at = page_.choice();
    if (pickingSteam_) {
        pickingSteam_ = false;
        scopes_.setSteamInput(target_.scope.id, at == 0   ? std::nullopt
                                                : at == 1 ? std::optional<bool>{true}
                                                          : std::optional<bool>{false});
        save();
    } else if (at < choices_.size()) {
        const Choice choice = choices_[at];
        if (const auto* button = std::get_if<Button>(&picking_)) {
            edit([&](input::ButtonMap& map) {
                return map.setButton(*button, std::get<input::PadOutput>(choice));
            });
        } else if (const auto* chord = std::get_if<input::PadChord>(&picking_)) {
            edit([&](input::ButtonMap& map) -> std::string {
                if (std::holds_alternative<Remove>(choice)) {
                    map.removeChord(*chord);
                    return {};
                }
                if (const auto* action = std::get_if<input::Action>(&choice)) {
                    return map.setChord(*chord, *action);
                }
                return map.setChord(*chord, std::get<input::PadOutput>(choice));
            });
        }
    }
    picking_ = std::monostate{};
    choices_.clear();
    page_.showBoard(note());
    show();
}

void MappingEditor::capturePad(const std::vector<gamepad::Event>& events) {
    for (const gamepad::Event& event : events) {
        const std::optional<ChordCapture::Heard> heard = capture_.feed(event);
        if (!heard) {
            continue;
        }
        if (heard->cancelled) {
            page_.showBoard(note());
            return;
        }
        const input::PadChord chord = heard->chord;
        if (!input::mappable(chord.modifier) || !input::mappable(chord.trigger)) {
            hooks_.say("Guide's chords are openSU's; hold another button", true);
            continue;
        }
        if (const std::optional<input::Action> shell = shell_.actionFor(chord)) {
            hooks_.say(input::describe(chord) + " is already " + std::string{input::label(*shell)},
                       true);
            continue;
        }
        pickChord(chord);
        return;
    }
}

void MappingEditor::edit(const std::function<std::string(input::ButtonMap&)>& change) {
    input::ButtonMap map = shown();
    if (const std::string refused = change(map); !refused.empty()) {
        hooks_.say(refused, true);
        return;
    }
    scopes_.set(target_.scope, std::move(map));
    save();
}

void MappingEditor::save() {
    std::string error;
    if (!store_.save(scopes_, error)) {
        lucent::error("settings", "cannot save {}: {}", store_.file().string(), error);
        hooks_.say("the mappings could not be saved", true);
    }
    hooks_.changed();
}

std::vector<ui::SettingsRow> MappingEditor::settingsRows() const {
    const std::vector<Emulator> emulators = hooks_.emulators();
    std::vector<ui::SettingsRow> rows;
    rows.push_back(ui::makeRow(std::string{globalSetting}, ui::RowKind::Action, "Button mapping",
                               "Every game's, unless it has its own",
                               scopes_.global() ? "Custom" : "None"));
    std::string value = "No emulators";
    if (!emulators.empty()) {
        const Emulator& picked = emulators[std::min(emulator_, emulators.size() - 1)];
        value = picked.name + (scopes_.emulators().contains(picked.id) ? " · custom" : "");
    }
    rows.push_back(ui::makeRow(std::string{emulatorSetting}, ui::RowKind::Choice,
                               "Emulator mapping", "Shared by the emulator's games", value));
    rows.push_back(ui::makeToggle(std::string{steamSetting}, "Steam Input",
                                  "Steam maps every game's buttons", scopes_.steamInputGlobal()));
    return rows;
}

bool MappingEditor::owns(std::string_view id) {
    return id == globalSetting || id == emulatorSetting || id == steamSetting;
}

void MappingEditor::runSetting(const std::string& id, int step) {
    if (id == globalSetting && step == 0) {
        openGlobal();
    } else if (id == emulatorSetting) {
        const std::vector<Emulator> emulators = hooks_.emulators();
        if (emulators.empty()) {
            return;
        }
        const std::size_t count = emulators.size();
        emulator_ = std::min(emulator_, count - 1);
        if (step == 0) {
            openEmulator(emulators[emulator_].id, emulators[emulator_].name);
            return;
        }
        emulator_ = step > 0 ? (emulator_ + 1) % count : (emulator_ + count - 1) % count;
        hooks_.changed();
    } else if (id == steamSetting) {
        scopes_.setSteamInputGlobal(!scopes_.steamInputGlobal());
        save();
    }
}

} // namespace opensu::app
