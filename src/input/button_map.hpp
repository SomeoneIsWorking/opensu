// button_map — one button mapping: what each pad button sends to the game, and the game chords (a
// held modifier and a partner) with what each does. A button sends virtual pad buttons, one,
// several pressed together, or none; a chord sends virtual pad buttons or performs a shell action.
// A button left out sends itself. Guide is openSU's and is never mapped. `forwardMap` turns a
// mapping into what the held pads apply. A keyboard output (`Combo`, for a standalone emulator's
// hotkeys) would be a third `ChordOutput` alternative; it is not built yet.
#pragma once

#include <map>
#include <set>
#include <string>
#include <variant>
#include <vector>

#include "gamepad/event.hpp"
#include "gamepad/forward_map.hpp"
#include "shortcuts.hpp"

namespace opensu::input {

/// Virtual pad buttons pressed together; empty sends nothing.
using PadOutput = std::set<gamepad::Button>;

/// What a game chord does: presses virtual pad buttons, or performs a shell action.
using ChordOutput = std::variant<PadOutput, Action>;

/// A game chord and what it does.
struct ChordMapping {
    PadChord chord;
    ChordOutput output;

    bool operator==(const ChordMapping&) const = default;
};

/// Whether a mapping can take `button` or send it: the pad buttons the virtual pad carries.
[[nodiscard]] bool mappable(gamepad::Button button) noexcept;

/// "B", "Select + Start", "Nothing".
[[nodiscard]] std::string describe(const PadOutput& output);
/// A pad output as `describe` names it, or the action's name.
[[nodiscard]] std::string describe(const ChordOutput& output);

class ButtonMap {
  public:
    /// What `button` sends.
    [[nodiscard]] PadOutput output(gamepad::Button button) const;
    /// The buttons that do not send themselves.
    [[nodiscard]] const std::map<gamepad::Button, PadOutput>& buttons() const noexcept {
        return buttons_;
    }
    [[nodiscard]] const std::vector<ChordMapping>& chords() const noexcept {
        return chords_;
    }
    /// What `chord` does, or null when the mapping has no such chord.
    [[nodiscard]] const ChordOutput* chord(const PadChord& chord) const;
    /// Whether every button sends itself and there are no chords.
    [[nodiscard]] bool empty() const noexcept {
        return buttons_.empty() && chords_.empty();
    }

    /// Makes `button` send `output`. Empty when done, else why it is refused.
    [[nodiscard]] std::string setButton(gamepad::Button button, PadOutput output);
    /// Makes `chord` do `output`, in place of what it did. Empty when done, else why it is refused.
    [[nodiscard]] std::string setChord(const PadChord& chord, ChordOutput output);
    void removeChord(const PadChord& chord);

    bool operator==(const ButtonMap&) const = default;

  private:
    std::map<gamepad::Button, PadOutput> buttons_;
    std::vector<ChordMapping> chords_;
};

/// What the held pads apply for `map`: its buttons and its chords that send pad buttons, less the
/// chords `shell` binds, which stay the shell's.
[[nodiscard]] gamepad::ForwardMap forwardMap(const ButtonMap& map, const Shortcuts& shell);

} // namespace opensu::input
