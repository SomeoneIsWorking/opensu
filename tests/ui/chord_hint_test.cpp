// The held modifier's chord list: exactly that modifier's chords, shown and hidden with the hold.
#include "chord_hint.hpp"

#include <cstdio>

#include "check.hpp"

namespace {

using namespace opensu::ui;
using opensu::gamepad::Button;
using opensu::input::Action;
using opensu::input::PadChord;
using opensu::input::Shortcuts;
using opensu::test::expect;
using Clock = ChordHint::Clock;
using Rows = std::vector<ChordHintRow>;

void rowsAreExactlyTheModifiersChords() {
    Shortcuts shortcuts;
    expect(chordHintRows(shortcuts, Button::Guide) == Rows{{"A", "Quick menu"},
                                                           {"UP", "Volume up"},
                                                           {"DOWN", "Volume down"},
                                                           {"LEFT", "Mute"}},
           "Guide lists the shipped chords: partner glyph and action");
    expect(shortcuts.rebind(Action::CloseGame, PadChord{Button::L2, Button::Start}).empty(),
           "Close game on L2 + Start");
    expect(chordHintRows(shortcuts, Button::L2) == Rows{{"+", "Close game"}},
           "L2 lists its one chord, Start by its glyph");
    expect(chordHintRows(shortcuts, Button::Guide).size() == 4, "and Guide does not list it");
    expect(chordHintRows(shortcuts, Button::B).empty(), "a button with no chords lists none");
}

void followsTheHold() {
    const Shortcuts shortcuts;
    ChordHint hint;
    const Clock::time_point start{std::chrono::seconds{10}};
    expect(!hint.visible(start) && !hint.modifier(), "nothing shown at first");
    hint.follow(Button::Guide, shortcuts, start);
    expect(hint.visible(start) && hint.modifier() == Button::Guide && hint.rows().size() == 4 &&
               hint.modifierGlyph() == "GUIDE",
           "a long hold shows the modifier's chords");
    const auto ended = start + std::chrono::seconds{1};
    hint.follow(std::nullopt, shortcuts, ended);
    expect(!hint.modifier() && hint.visible(ended) && hint.rows().size() == 4,
           "its end starts the fade, the list kept while it fades");
    expect(!hint.visible(ended + std::chrono::seconds{1}), "and it is gone");
}

} // namespace

int main() {
    rowsAreExactlyTheModifiersChords();
    followsTheHold();
    std::printf("chord_hint: all checks passed\n");
    return 0;
}
