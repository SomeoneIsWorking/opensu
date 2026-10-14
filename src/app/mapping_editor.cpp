#include "mapping_editor.hpp"

#include <algorithm>
#include <type_traits>

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
constexpr std::string_view listeningNote = "Hold a button and press another; B alone cancels";
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
    page_.open(target_.title, rows(), note());
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

std::vector<ui::MappingRow> MappingEditor::rows() const {
    std::vector<ui::MappingRow> rows;
    if (target_.scope.kind == Kind::Game) {
        const std::optional<bool> own = scopes_.steamInputOf(target_.scope.id);
        rows.push_back(ui::MappingRow{std::string{steamRow},
                                      {},
                                      "Steam Input",
                                      own ? onOff(*own)
                                          : onOff(scopes_.steamInputGlobal()) + " (every game)"});
    }
    const input::ButtonMap map = shown();
    for (const Button button : gamepad::padButtons) {
        rows.push_back(ui::MappingRow{std::string{buttonRow} + std::string{gamepad::name(button)},
                                      {std::string{input::glyphOf(button)}},
                                      input::buttonLabel(button),
                                      input::describe(map.output(button))});
    }
    for (const input::ChordMapping& mapping : map.chords()) {
        rows.push_back(ui::MappingRow{chordId(mapping.chord),
                                      {std::string{input::glyphOf(mapping.chord.modifier)},
                                       std::string{input::glyphOf(mapping.chord.trigger)}},
                                      input::describe(mapping.chord),
                                      input::describe(mapping.output)});
    }
    rows.push_back(ui::MappingRow{std::string{addRow}, {}, "Add a chord", ""});
    if (scopes_.at(target_.scope) != nullptr) {
        rows.push_back(ui::MappingRow{std::string{resetRow},
                                      {},
                                      target_.scope.kind == Kind::Global
                                          ? "Clear the mapping"
                                          : "Use the mapping it would inherit",
                                      ""});
    }
    return rows;
}

void MappingEditor::show() {
    page_.refresh(rows(), note());
}

void MappingEditor::act(Button button) {
    if (!page_.isOpen() || page_.mode() == ui::MappingMode::Listening) {
        return;
    }
    const bool picking = page_.mode() == ui::MappingMode::Picking;
    switch (button) {
    case Button::Up:
    case Button::Down:
        // input-sound.md 3.4 Navigation: a focus move in a list.
        if (page_.move(button == Button::Down ? 1 : -1)) {
            sounds_.play(audio::Effect::Navigation);
        }
        break;
    case Button::A:
        if (picking) {
            choose();
        } else if (const ui::MappingRow* row = page_.focused()) {
            runRow(*row);
        }
        break;
    case Button::B:
        if (picking) {
            picking_ = std::monostate{};
            pickingSteam_ = false;
            page_.showRows(note());
        } else {
            close();
        }
        break;
    default:
        break;
    }
}

void MappingEditor::runRow(const ui::MappingRow& row) {
    if (row.id == steamRow) {
        pickingSteam_ = true;
        const std::optional<bool> own = scopes_.steamInputOf(target_.scope.id);
        const std::size_t at = !own ? 0 : *own ? 1 : 2;
        page_.pick("Steam Input",
                   {"As every game (" + onOff(scopes_.steamInputGlobal()) + ")", "On", "Off"}, at);
        return;
    }
    if (row.id == resetRow) {
        scopes_.clear(target_.scope);
        save();
        show();
        return;
    }
    if (steamInputHasIt()) {
        hooks_.say(std::string{steamRefusal}, true);
        return;
    }
    if (row.id == addRow) {
        capture_.begin();
        page_.listen(std::string{listeningNote});
    } else if (row.id.starts_with(buttonRow)) {
        pickButton(gamepad::buttonNamed(std::string_view{row.id}.substr(buttonRow.size())));
    } else if (const std::optional<input::PadChord> chord = chordOfId(row.id)) {
        pickChord(*chord);
    }
}

void MappingEditor::pickButton(Button button) {
    std::vector<Choice> choices;
    choices.reserve(gamepad::padButtons.size() + 2);
    for (const Button out : gamepad::padButtons) {
        choices.emplace_back(input::PadOutput{out});
    }
    choices.emplace_back(input::PadOutput{});
    choices.emplace_back(input::PadOutput{Button::Select, Button::Start});
    picking_ = button;
    pick(input::buttonLabel(button) + " sends", std::move(choices), Choice{shown().output(button)});
}

void MappingEditor::pickChord(const input::PadChord& chord) {
    std::vector<Choice> choices;
    for (const input::Action action : input::allActions) {
        if (input::takesPadChord(action)) {
            choices.emplace_back(action);
        }
    }
    for (const Button out : gamepad::padButtons) {
        choices.emplace_back(input::PadOutput{out});
    }
    for (const Button out : gamepad::padButtons) {
        if (out != Button::Select) {
            choices.emplace_back(input::PadOutput{Button::Select, out});
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
        choices.emplace_back(Remove{});
    }
    picking_ = chord;
    pick(input::describe(chord) + " does", std::move(choices), current);
}

void MappingEditor::pick(std::string title, std::vector<Choice> choices,
                         const std::optional<Choice>& current) {
    const auto found = current ? std::ranges::find(choices, *current) : choices.end();
    const std::size_t at =
        found == choices.end() ? 0 : static_cast<std::size_t>(found - choices.begin());
    std::vector<std::string> labels;
    labels.reserve(choices.size());
    for (const Choice& choice : choices) {
        labels.push_back(std::visit(
            [](const auto& value) -> std::string {
                using Value = std::decay_t<decltype(value)>;
                if constexpr (std::is_same_v<Value, Remove>) {
                    return "Remove this chord";
                } else {
                    return input::describe(input::ChordOutput{value});
                }
            },
            choice));
    }
    choices_ = std::move(choices);
    page_.pick(std::move(title), std::move(labels), at);
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
    page_.showRows(note());
    show();
}

void MappingEditor::capturePad(const std::vector<gamepad::Event>& events) {
    for (const gamepad::Event& event : events) {
        const std::optional<ChordCapture::Heard> heard = capture_.feed(event);
        if (!heard) {
            continue;
        }
        if (heard->cancelled) {
            page_.showRows(note());
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
