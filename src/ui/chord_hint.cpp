#include "chord_hint.hpp"

namespace opensu::ui {

std::vector<ChordHintRow> chordHintRows(const input::ChordSet& chords, gamepad::Button modifier) {
    std::vector<ChordHintRow> rows;
    for (const input::ChordListing& listing : chords.on(modifier)) {
        rows.push_back(
            ChordHintRow{std::string{input::glyphOf(listing.chord.trigger)}, listing.label});
    }
    return rows;
}

void ChordHint::follow(std::optional<gamepad::Button> modifier, const input::ChordSet& chords,
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
    rows_ = chordHintRows(chords, *modifier);
    fade_.show(now);
}

} // namespace opensu::ui
