// chord_set — the pad chords in force: the shell's (`Shortcuts`) and, while a game runs, the chords
// of the game's mapping. A chord both bind stays the shell's. The shell's gates play them
// (`PadChords`), the pads keep them from the game (`triggers`), and the chord list shows them
// (`on`).
#pragma once

#include <chrono>
#include <optional>
#include <string>
#include <vector>

#include "button_map.hpp"
#include "gamepad/chord_gate.hpp"
#include "shortcuts.hpp"

namespace opensu::input {

/// How long a modifier held alone may be and still be a tap when released. Longer, its release
/// does nothing.
inline constexpr std::chrono::milliseconds holdDelay{500};

/// One chord on a modifier, as the chord list shows it.
struct ChordListing {
    PadChord chord;
    /// The action's name, or what the game chord sends.
    std::string label;
    /// From the game's mapping, not the shell's table.
    bool game{false};

    bool operator==(const ChordListing&) const = default;
};

class ChordSet {
  public:
    explicit ChordSet(const Shortcuts& shell) noexcept : shell_{&shell} {
    }

    [[nodiscard]] const Shortcuts& shell() const noexcept {
        return *shell_;
    }

    /// The running game's chords from its mapping; none outside a game.
    void setGame(std::vector<ChordMapping> chords) {
        game_ = std::move(chords);
    }
    [[nodiscard]] const std::vector<ChordMapping>& game() const noexcept {
        return game_;
    }

    /// Every chord in force, with the hold delay: what a gate needs.
    [[nodiscard]] gamepad::ChordTriggers triggers() const;
    /// The action `chord` performs: the shell's, else the game chord's. Nothing for a game chord
    /// that sends pad buttons, which the pads play, or for no chord.
    [[nodiscard]] std::optional<Action> actionFor(const PadChord& chord) const;
    /// Every chord held on `modifier`: the shell's in action order, then the game's.
    [[nodiscard]] std::vector<ChordListing> on(gamepad::Button modifier) const;

  private:
    const Shortcuts* shell_;
    std::vector<ChordMapping> game_;
};

} // namespace opensu::input
