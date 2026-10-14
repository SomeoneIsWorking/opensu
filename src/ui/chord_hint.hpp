// chord_hint — the list of a held modifier's chords: shown once the modifier has been held alone
// for the hold delay (`input::PadChords::longHeld`), hidden when that hold ends. iiSU has no hold
// (docs/reference/iisu/input-sound.md); this is openSU's own, after SteamOS's held-STEAM list.
#pragma once

#include <chrono>
#include <optional>
#include <string>
#include <vector>

#include "gamepad/event.hpp"
#include "input/shortcuts.hpp"
#include "panel_fade.hpp"

namespace opensu::ui {

/// One chord on the list: the partner button's glyph key and what the chord does.
struct ChordHintRow {
    std::string glyph;
    std::string label;

    bool operator==(const ChordHintRow&) const = default;
};

/// The rows for every chord `shortcuts` binds on `modifier`, in action order.
[[nodiscard]] std::vector<ChordHintRow> chordHintRows(const input::Shortcuts& shortcuts,
                                                      gamepad::Button modifier);

class ChordHint {
  public:
    using Clock = std::chrono::steady_clock;

    /// Follows the long hold: `modifier`'s chords are shown while it is held, nothing once it is
    /// not. A change of modifier replaces the list.
    void follow(std::optional<gamepad::Button> modifier, const input::Shortcuts& shortcuts,
                Clock::time_point now);

    /// The modifier whose chords are up, or nothing while hidden or fading out.
    [[nodiscard]] std::optional<gamepad::Button> modifier() const noexcept {
        return modifier_;
    }
    /// The list shown, kept while it fades out.
    [[nodiscard]] const std::vector<ChordHintRow>& rows() const noexcept {
        return rows_;
    }
    /// The held modifier's glyph key, kept while it fades out.
    [[nodiscard]] const std::string& modifierGlyph() const noexcept {
        return modifierGlyph_;
    }
    [[nodiscard]] PanelLook look(Clock::time_point now) const noexcept {
        return fade_.look(now);
    }
    /// Whether anything is drawn: shown, or still fading out.
    [[nodiscard]] bool visible(Clock::time_point now) const noexcept {
        return fade_.visible(now);
    }

  private:
    PanelFade fade_{FadeSpec{110.0f, 160.0f, 0.96f, 140.0f, 0.985f}};
    std::optional<gamepad::Button> modifier_;
    std::string modifierGlyph_;
    std::vector<ChordHintRow> rows_;
};

} // namespace opensu::ui
