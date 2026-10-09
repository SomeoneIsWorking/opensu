// shortcuts — the one table from key combinations and pad chords to the actions they perform.
// The shell reads its keys from it, the game-time key watcher asks it, the prompts name their caps
// from it, and the Settings screen remaps it. `Shortcuts` holds the defaults with the player's
// overrides on top; the overrides are what the settings file keeps.
#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "gamepad/event.hpp"
#include "key_names.hpp"

namespace opensu::input {

class KeySource;

/// What a shortcut does. Every action has a handler in the shell.
enum class Action : std::uint8_t {
    Up,
    Down,
    Left,
    Right,
    Confirm,
    Back,
    X,
    Y,
    Select,
    Start,
    L1,
    R1,
    Search,
    /// The Guide menu over a running game.
    Guide,
    VolumeUp,
    VolumeDown,
    VolumeMute,
    /// Closes openSU.
    Quit,
};

inline constexpr std::array allActions{
    Action::Up,         Action::Down,       Action::Left,   Action::Right,  Action::Confirm,
    Action::Back,       Action::X,          Action::Y,      Action::Select, Action::Start,
    Action::L1,         Action::R1,         Action::Search, Action::Guide,  Action::VolumeUp,
    Action::VolumeDown, Action::VolumeMute, Action::Quit};

/// The action's name in the Settings screen.
[[nodiscard]] std::string_view label(Action action) noexcept;
/// The action's spelling in the settings file.
[[nodiscard]] std::string_view spelling(Action action) noexcept;
[[nodiscard]] std::optional<Action> actionOf(std::string_view spelling) noexcept;
/// The pad button the action presses, for the actions that are a button.
[[nodiscard]] std::optional<gamepad::Button> buttonOf(Action action) noexcept;
/// Whether the action also works while a game has the keyboard, where only these are watched for.
[[nodiscard]] bool worksInGame(Action action) noexcept;
/// Whether a pad chord can be set for the action.
[[nodiscard]] bool takesPadChord(Action action) noexcept;

/// A pad button held (L2, R2, L3 or R3) while another is pressed.
struct PadChord {
    gamepad::Button modifier{gamepad::Button::None};
    gamepad::Button trigger{gamepad::Button::None};

    bool operator==(const PadChord&) const = default;
};

/// Buttons whose own press does nothing in the shell, so they can be held for a chord.
[[nodiscard]] bool canHold(gamepad::Button button) noexcept;

/// `L2 + Up`: a name for the chord.
[[nodiscard]] std::string describe(const PadChord& chord);

/// One key binding.
struct KeyBinding {
    Action action;
    Combo combo;
};

/// The player's changes to the defaults: the combination an action's first binding is replaced by,
/// and the pad chord an action has.
struct ShortcutOverrides {
    std::map<Action, Combo> keys;
    std::map<Action, PadChord> pads;

    bool operator==(const ShortcutOverrides&) const = default;
};

/// A shortcut key going down or up.
struct ActionEdge {
    Action action;
    bool pressed;
};

class Shortcuts {
  public:
    explicit Shortcuts(ShortcutOverrides overrides = {});

    [[nodiscard]] const ShortcutOverrides& overrides() const noexcept {
        return overrides_;
    }
    /// Every key binding, each action's first being the one its prompts show.
    [[nodiscard]] std::span<const KeyBinding> bindings() const noexcept {
        return bindings_;
    }
    /// The combination the action's prompts show.
    [[nodiscard]] std::optional<Combo> primary(Action action) const;
    /// The pad chord of the action, or nothing for none.
    [[nodiscard]] std::optional<PadChord> padChord(Action action) const;

    /// The action a combination performs, or nothing.
    [[nodiscard]] std::optional<Action> actionFor(const Combo& combo) const;
    [[nodiscard]] std::optional<Action> actionFor(const PadChord& chord) const;

    /// The key edges the keys in `keys` made this frame. A key reports its edges like a pad's
    /// button, so a held arrow repeats on the pad's schedule.
    [[nodiscard]] std::vector<ActionEdge> keyEdges(const KeySource& keys) const;

    /// The cap a button's prompt shows (`Enter`, `Ctrl+F`), or nothing when no key reaches it.
    [[nodiscard]] std::optional<std::string> keyLabelFor(gamepad::Button button) const;
    /// The cap for a glyph key ("A", "LB"), or nothing when its button has no key.
    [[nodiscard]] std::optional<std::string> keyLabelForGlyph(std::string_view glyph) const;

    /// Makes `combo` the action's first binding. Empty when done, else why it is refused: it is a
    /// modifier alone, or another action has it.
    [[nodiscard]] std::string rebind(Action action, const Combo& combo);
    /// Makes `chord` the action's pad chord. Empty when done, else why it is refused.
    [[nodiscard]] std::string rebind(Action action, const PadChord& chord);
    /// Puts the action's key and pad chord back to the defaults.
    void reset(Action action);
    void resetAll();

  private:
    void build();

    ShortcutOverrides overrides_;
    std::vector<KeyBinding> bindings_;
    std::map<Action, PadChord> pads_;
};

/// The button of a glyph key: "A", "B", "X", "Y", "-", "+", "LB", "RB".
[[nodiscard]] gamepad::Button buttonOfGlyph(std::string_view glyph) noexcept;

} // namespace opensu::input
