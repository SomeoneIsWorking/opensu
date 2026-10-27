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

#include "gamepad/chord_gate.hpp"
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
    /// The Guide menu: the left menu, over a running game too.
    Guide,
    /// The Guide menu on Volume, over a running game too.
    QuickMenu,
    VolumeUp,
    VolumeDown,
    VolumeMute,
    /// Closes openSU.
    Quit,
    /// The game actions, for a chord over a running game; save, load and reset need opensu-retro.
    CloseGame,
    SaveState,
    LoadState,
    ResetGame,
    /// The Devices page.
    Devices,
    /// The home screen.
    Home,
    /// The Downloads page.
    Downloads,
};

inline constexpr std::array allActions{
    Action::Up,        Action::Down,       Action::Left,       Action::Right,   Action::Confirm,
    Action::Back,      Action::X,          Action::Y,          Action::Select,  Action::Start,
    Action::L1,        Action::R1,         Action::Search,     Action::Guide,   Action::QuickMenu,
    Action::VolumeUp,  Action::VolumeDown, Action::VolumeMute, Action::Quit,    Action::CloseGame,
    Action::SaveState, Action::LoadState,  Action::ResetGame,  Action::Devices, Action::Home,
    Action::Downloads};

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

/// A pad button held while another is pressed. Any button can be the held one; a tap of it still
/// does what the button does (`gamepad::ChordGate`).
using PadChord = gamepad::Chord;

/// A button's name for the player: "A", "L2", "Select".
[[nodiscard]] std::string buttonLabel(gamepad::Button button);
/// `L2 + Up`: a name for the chord.
[[nodiscard]] std::string describe(const PadChord& chord);

/// An action and the pad chord that plays it.
struct ChordBinding {
    Action action;
    PadChord chord;

    bool operator==(const ChordBinding&) const = default;
};

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
    /// Every chord held on `modifier`, in action order.
    [[nodiscard]] std::vector<ChordBinding> chordsOn(gamepad::Button modifier) const;

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
    /// Puts every pad chord back to the defaults, keys as they are.
    void resetPads();

  private:
    void build();

    ShortcutOverrides overrides_;
    std::vector<KeyBinding> bindings_;
    std::map<Action, PadChord> pads_;
};

/// The glyph key `ui::ButtonGlyphPainter` draws for a button: "A", "-" for Select, "+" for Start,
/// "LB", "LT", "UP", and the button's prompt for the rest.
[[nodiscard]] std::string_view glyphOf(gamepad::Button button);
/// The button of a glyph key, or `None`.
[[nodiscard]] gamepad::Button buttonOfGlyph(std::string_view glyph);

} // namespace opensu::input
