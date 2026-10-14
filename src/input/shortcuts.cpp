#include "shortcuts.hpp"

#include <algorithm>
#include <cctype>
#include <type_traits>

#include "keyboard_bindings.hpp"

namespace opensu::input {
namespace {

using gamepad::Button;

struct ActionInfo {
    Action action;
    const char* spelling;
    const char* label;
    /// Presses this pad button in the shell.
    Button button;
    bool inGame;
    bool padChord;
};

constexpr std::array<ActionInfo, allActions.size()> infos{{
    {Action::Up, "up", "Up", Button::Up, false, false},
    {Action::Down, "down", "Down", Button::Down, false, false},
    {Action::Left, "left", "Left", Button::Left, false, false},
    {Action::Right, "right", "Right", Button::Right, false, false},
    {Action::Confirm, "confirm", "Confirm", Button::A, false, false},
    {Action::Back, "back", "Back", Button::B, false, false},
    {Action::X, "x", "Refresh", Button::X, false, false},
    {Action::Y, "y", "Details", Button::Y, false, false},
    {Action::Select, "select", "Menu", Button::Select, false, false},
    {Action::Start, "start", "Options", Button::Start, false, false},
    {Action::L1, "l1", "Previous section", Button::L1, false, false},
    {Action::R1, "r1", "Next section", Button::R1, false, false},
    {Action::Search, "search", "Search", Button::Search, false, false},
    {Action::Guide, "guide", "Guide menu", Button::Guide, true, false},
    {Action::QuickMenu, "quickMenu", "Quick menu", Button::None, true, true},
    {Action::VolumeUp, "volumeUp", "Volume up", Button::None, true, true},
    {Action::VolumeDown, "volumeDown", "Volume down", Button::None, true, true},
    {Action::VolumeMute, "volumeMute", "Mute", Button::None, true, true},
    {Action::Quit, "quit", "Quit openSU", Button::None, false, false},
    {Action::CloseGame, "closeGame", "Close game", Button::None, true, true},
    {Action::SaveState, "saveState", "Save state", Button::None, true, true},
    {Action::LoadState, "loadState", "Load state", Button::None, true, true},
    {Action::ResetGame, "resetGame", "Reset game", Button::None, true, true},
    {Action::Devices, "devices", "Devices", Button::None, true, true},
    {Action::Home, "home", "Home", Button::None, true, true},
}};

static_assert(std::ranges::all_of(allActions,
                                  [](Action action) {
                                      return infos[static_cast<std::size_t>(action)].action ==
                                             action;
                                  }),
              "infos is in the order of Action");

const ActionInfo& infoOf(Action action) noexcept {
    return infos[static_cast<std::size_t>(action)];
}

/// The shipped key bindings; an action's first entry is the one its prompts show.
constexpr std::array<KeyBinding, 28> defaultBindings{{
    {Action::Up, {keys::Up}},
    {Action::Up, {keys::W}},
    {Action::Down, {keys::Down}},
    {Action::Down, {keys::S}},
    {Action::Left, {keys::Left}},
    {Action::Left, {keys::A}},
    {Action::Right, {keys::Right}},
    {Action::Right, {keys::D}},
    {Action::Confirm, {keys::Enter}},
    {Action::Confirm, {keys::Space}},
    {Action::Back, {keys::Escape}},
    {Action::X, {keys::F}},
    {Action::Y, {keys::Y}},
    {Action::Select, {keys::Tab}},
    {Action::Select, {keys::Menu}},
    {Action::Select, {keys::F10, false, true}},
    {Action::Start, {keys::E}},
    {Action::L1, {keys::LeftBracket}},
    {Action::R1, {keys::RightBracket}},
    {Action::R1, {keys::R}},
    {Action::Search, {keys::Slash}},
    {Action::Search, {keys::F, true, false}},
    {Action::Guide, {keys::Tab, false, true}},
    {Action::QuickMenu, {keys::Tab, true, false}},
    {Action::VolumeUp, {keys::Up, true, false}},
    {Action::VolumeDown, {keys::Down, true, false}},
    {Action::VolumeMute, {keys::M, true, false}},
    {Action::Quit, {keys::Q}},
}};

struct DefaultPad {
    Action action;
    PadChord chord;
};

constexpr std::array<DefaultPad, 4> defaultPads{{
    {Action::QuickMenu, {Button::Guide, Button::A}},
    {Action::VolumeUp, {Button::Guide, Button::Up}},
    {Action::VolumeDown, {Button::Guide, Button::Down}},
    {Action::VolumeMute, {Button::Guide, Button::Left}},
}};

std::string capitalised(std::string_view word) {
    std::string out{word};
    if (!out.empty()) {
        out.front() = static_cast<char>(std::toupper(static_cast<unsigned char>(out.front())));
    }
    return out;
}

} // namespace

std::string_view label(Action action) noexcept {
    return infoOf(action).label;
}

std::string_view spelling(Action action) noexcept {
    return infoOf(action).spelling;
}

std::optional<Action> actionOf(std::string_view text) noexcept {
    for (const ActionInfo& info : infos) {
        if (text == info.spelling) {
            return info.action;
        }
    }
    return std::nullopt;
}

std::optional<Button> buttonOf(Action action) noexcept {
    const Button button = infoOf(action).button;
    return button == Button::None ? std::nullopt : std::optional<Button>{button};
}

bool worksInGame(Action action) noexcept {
    return infoOf(action).inGame;
}

bool takesPadChord(Action action) noexcept {
    return infoOf(action).padChord;
}

std::string describe(const PadChord& chord) {
    return capitalised(gamepad::name(chord.modifier)) + " + " +
           capitalised(gamepad::name(chord.trigger));
}

Shortcuts::Shortcuts(ShortcutOverrides overrides) : overrides_{std::move(overrides)} {
    build();
}

void Shortcuts::build() {
    bindings_.assign(defaultBindings.begin(), defaultBindings.end());
    pads_.clear();
    for (const DefaultPad& pad : defaultPads) {
        pads_[pad.action] = pad.chord;
    }
    for (const auto& [action, combo] : overrides_.keys) {
        const auto first = std::ranges::find(bindings_, action, &KeyBinding::action);
        if (first != bindings_.end()) {
            first->combo = combo;
        }
    }
    for (const auto& [action, chord] : overrides_.pads) {
        pads_[action] = chord;
    }
}

std::optional<Combo> Shortcuts::primary(Action action) const {
    const auto first = std::ranges::find(bindings_, action, &KeyBinding::action);
    return first == bindings_.end() ? std::nullopt : std::optional<Combo>{first->combo};
}

std::optional<PadChord> Shortcuts::padChord(Action action) const {
    const auto found = pads_.find(action);
    return found == pads_.end() ? std::nullopt : std::optional<PadChord>{found->second};
}

std::vector<ChordBinding> Shortcuts::chordsOn(Button modifier) const {
    std::vector<ChordBinding> chords;
    for (const auto& [action, chord] : pads_) {
        if (chord.modifier == modifier) {
            chords.push_back(ChordBinding{action, chord});
        }
    }
    return chords;
}

std::optional<Action> Shortcuts::actionFor(const Combo& combo) const {
    for (const KeyBinding& binding : bindings_) {
        if (binding.combo == combo) {
            return binding.action;
        }
    }
    return std::nullopt;
}

std::optional<Action> Shortcuts::actionFor(const PadChord& chord) const {
    for (const auto& [action, own] : pads_) {
        if (own == chord) {
            return action;
        }
    }
    return std::nullopt;
}

std::vector<ActionEdge> Shortcuts::keyEdges(const KeySource& keys) const {
    const bool ctrl = keys.down(keys::LeftControl) || keys.down(keys::RightControl);
    const bool shift = keys.down(keys::LeftShift) || keys.down(keys::RightShift);
    std::vector<ActionEdge> edges;
    for (const KeyBinding& binding : bindings_) {
        if (binding.combo.ctrl != ctrl || binding.combo.shift != shift) {
            continue;
        }
        if (keys.pressed(binding.combo.key)) {
            edges.push_back({binding.action, true});
        } else if (keys.released(binding.combo.key)) {
            edges.push_back({binding.action, false});
        }
    }
    return edges;
}

std::optional<std::string> Shortcuts::keyLabelFor(Button button) const {
    for (const KeyBinding& binding : bindings_) {
        if (buttonOf(binding.action) == button) {
            return describe(binding.combo);
        }
    }
    return std::nullopt;
}

std::optional<std::string> Shortcuts::keyLabelForGlyph(std::string_view glyph) const {
    const Button button = buttonOfGlyph(glyph);
    if (button == Button::None) {
        return std::nullopt;
    }
    return keyLabelFor(button);
}

std::string Shortcuts::rebind(Action action, const Combo& combo) {
    if (modifierOf(combo.key) != Modifier::Neither || !keyName(combo.key)) {
        return "that key cannot be a shortcut";
    }
    if (const std::optional<Action> holder = actionFor(combo); holder && *holder != action) {
        return describe(combo) + " is already " + std::string{label(*holder)};
    }
    if (primary(action) == combo) {
        return {};
    }
    overrides_.keys[action] = combo;
    const auto shipped = std::ranges::find(defaultBindings, action, &KeyBinding::action);
    if (shipped != defaultBindings.end() && shipped->combo == combo) {
        overrides_.keys.erase(action);
    }
    build();
    return {};
}

std::string Shortcuts::rebind(Action action, const PadChord& chord) {
    if (!takesPadChord(action)) {
        return std::string{label(action)} + " has no pad chord";
    }
    if (chord.modifier == Button::None || chord.trigger == Button::None ||
        chord.trigger == chord.modifier) {
        return "hold a button and press another";
    }
    if (const std::optional<Action> holder = actionFor(chord); holder && *holder != action) {
        return describe(chord) + " is already " + std::string{label(*holder)};
    }
    overrides_.pads[action] = chord;
    const auto shipped = std::ranges::find(defaultPads, action, &DefaultPad::action);
    if (shipped != defaultPads.end() && shipped->chord == chord) {
        overrides_.pads.erase(action);
    }
    build();
    return {};
}

void Shortcuts::reset(Action action) {
    overrides_.keys.erase(action);
    overrides_.pads.erase(action);
    build();
}

void Shortcuts::resetAll() {
    overrides_ = {};
    build();
}

void Shortcuts::resetPads() {
    overrides_.pads.clear();
    build();
}

std::string_view glyphOf(Button button) {
    switch (button) {
    case Button::Select:
        return "-";
    case Button::Start:
        return "+";
    default:
        return gamepad::prompt(button);
    }
}

Button buttonOfGlyph(std::string_view glyph) {
    using Value = std::underlying_type_t<Button>;
    for (auto value = static_cast<Value>(Button::A); value <= static_cast<Value>(Button::Search);
         ++value) {
        const auto button = static_cast<Button>(value);
        if (glyphOf(button) == glyph) {
            return button;
        }
    }
    return Button::None;
}

} // namespace opensu::input
