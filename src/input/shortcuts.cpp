#include "shortcuts.hpp"

#include <algorithm>
#include <cctype>

#include "keyboard_bindings.hpp"
#include "raylib.h"

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

constexpr std::array<ActionInfo, 18> infos{{
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
    {Action::VolumeUp, "volumeUp", "Volume up", Button::None, true, true},
    {Action::VolumeDown, "volumeDown", "Volume down", Button::None, true, true},
    {Action::VolumeMute, "volumeMute", "Mute", Button::None, true, true},
    {Action::Quit, "quit", "Quit openSU", Button::None, false, false},
}};

const ActionInfo& infoOf(Action action) noexcept {
    return infos[static_cast<std::size_t>(action)];
}

/// The shipped key bindings; an action's first entry is the one its prompts show.
constexpr std::array<KeyBinding, 27> defaultBindings{{
    {Action::Up, {KEY_UP}},
    {Action::Up, {KEY_W}},
    {Action::Down, {KEY_DOWN}},
    {Action::Down, {KEY_S}},
    {Action::Left, {KEY_LEFT}},
    {Action::Left, {KEY_A}},
    {Action::Right, {KEY_RIGHT}},
    {Action::Right, {KEY_D}},
    {Action::Confirm, {KEY_ENTER}},
    {Action::Confirm, {KEY_SPACE}},
    {Action::Back, {KEY_ESCAPE}},
    {Action::X, {KEY_F}},
    {Action::Y, {KEY_Y}},
    {Action::Select, {KEY_TAB}},
    {Action::Select, {KEY_KB_MENU}},
    {Action::Select, {KEY_F10, false, true}},
    {Action::Start, {KEY_E}},
    {Action::L1, {KEY_LEFT_BRACKET}},
    {Action::R1, {KEY_RIGHT_BRACKET}},
    {Action::R1, {KEY_R}},
    {Action::Search, {KEY_SLASH}},
    {Action::Search, {KEY_F, true, false}},
    {Action::Guide, {KEY_TAB, false, true}},
    {Action::VolumeUp, {KEY_UP, true, false}},
    {Action::VolumeDown, {KEY_DOWN, true, false}},
    {Action::VolumeMute, {KEY_M, true, false}},
    {Action::Quit, {KEY_Q}},
}};

struct DefaultPad {
    Action action;
    PadChord chord;
};

constexpr std::array<DefaultPad, 3> defaultPads{{
    {Action::VolumeUp, {Button::L2, Button::Up}},
    {Action::VolumeDown, {Button::L2, Button::Down}},
    {Action::VolumeMute, {Button::L2, Button::Left}},
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

bool canHold(Button button) noexcept {
    return button == Button::L2 || button == Button::R2 || button == Button::L3 ||
           button == Button::R3;
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
    const bool ctrl = keys.down(KEY_LEFT_CONTROL) || keys.down(KEY_RIGHT_CONTROL);
    const bool shift = keys.down(KEY_LEFT_SHIFT) || keys.down(KEY_RIGHT_SHIFT);
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
    if (!canHold(chord.modifier) || chord.trigger == Button::None ||
        chord.trigger == chord.modifier) {
        return "hold L2, R2, L3 or R3 and press another button";
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

Button buttonOfGlyph(std::string_view glyph) noexcept {
    if (glyph == "A") {
        return Button::A;
    }
    if (glyph == "B") {
        return Button::B;
    }
    if (glyph == "X") {
        return Button::X;
    }
    if (glyph == "Y") {
        return Button::Y;
    }
    if (glyph == "-") {
        return Button::Select;
    }
    if (glyph == "+") {
        return Button::Start;
    }
    if (glyph == "LB") {
        return Button::L1;
    }
    if (glyph == "RB") {
        return Button::R1;
    }
    return Button::None;
}

} // namespace opensu::input
