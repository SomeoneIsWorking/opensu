#include "chord_hint.hpp"

namespace opensu::ui {

std::vector<ChordHintRow> chordHintRows(const input::Shortcuts& shortcuts,
                                        gamepad::Button modifier) {
    std::vector<ChordHintRow> rows;
    for (const input::ChordBinding& binding : shortcuts.chordsOn(modifier)) {
        rows.push_back(ChordHintRow{std::string{input::glyphOf(binding.chord.trigger)},
                                    std::string{input::label(binding.action)}});
    }
    return rows;
}

void ChordHint::follow(std::optional<gamepad::Button> modifier, const input::Shortcuts& shortcuts,
                       Clock::time_point now) {
    if (modifier == modifier_) {
        return;
    }
    modifier_ = modifier;
    if (!modifier) {
        fade_.hide(now);
        return;
    }
    modifierGlyph_ = input::glyphOf(*modifier);
    rows_ = chordHintRows(shortcuts, *modifier);
    fade_.show(now);
}

} // namespace opensu::ui
