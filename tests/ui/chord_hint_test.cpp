// The held modifier's chord list: exactly that modifier's chords, shown and hidden with the hold.
#include "chord_hint.hpp"

#include <cstdio>

#include "check.hpp"
#include "support/test_main.hpp"

namespace {

using namespace opensu::ui;
using opensu::gamepad::Button;
using opensu::input::Action;
using opensu::input::ChordMapping;
using opensu::input::ChordSet;
using opensu::input::PadChord;
using opensu::input::PadOutput;
using opensu::input::Shortcuts;
using opensu::test::expect;
using Clock = ChordHint::Clock;
using Rows = std::vector<ChordHintRow>;

void rowsAreExactlyTheModifiersChords() {
    Shortcuts shortcuts;
    const ChordSet chords{shortcuts};
    expect(chordHintRows(chords, Button::Guide) == Rows{{"A", "Quick menu"},
                                                        {"UP", "Volume up"},
                                                        {"DOWN", "Volume down"},
                                                        {"LEFT", "Mute"}},
           "Guide lists the shipped chords: partner glyph and action");
    expect(shortcuts.rebind(Action::CloseGame, PadChord{Button::L2, Button::Start}).empty(),
           "Close game on L2 + Start");
    expect(chordHintRows(chords, Button::L2) == Rows{{"+", "Close game"}},
           "L2 lists its one chord, Start by its glyph");
    expect(chordHintRows(chords, Button::Guide).size() == 4, "and Guide does not list it");
    expect(chordHintRows(chords, Button::B).empty(), "a button with no chords lists none");
}

void rowsListTheGamesChords() {
    Shortcuts shortcuts;
    expect(shortcuts.rebind(Action::CloseGame, PadChord{Button::L2, Button::Start}).empty(),
           "Close game on L2 + Start");
    ChordSet chords{shortcuts};
    chords.setGame({ChordMapping{PadChord{Button::L2, Button::Y}, Action::SaveState},
                    ChordMapping{PadChord{Button::L2, Button::A}, PadOutput{Button::Select}},
                    ChordMapping{PadChord{Button::L2, Button::Start}, PadOutput{Button::X}}});
    expect(chordHintRows(chords, Button::L2) ==
               Rows{{"+", "Close game"}, {"Y", "Save state"}, {"A", "Sends Select"}},
           "L2 lists the shell's chord, then the game's, and not the one the shell has");
    ChordHint hint;
    hint.follow(Button::L2, chords, Clock::time_point{});
    expect(hint.rows().size() == 3, "the list over a game shows the game's chords");
}

void followsTheHold() {
    const Shortcuts shortcuts;
    const ChordSet chords{shortcuts};
    ChordHint hint;
    const Clock::time_point start{std::chrono::seconds{10}};
    expect(!hint.visible(start) && !hint.modifier(), "nothing shown at first");
    hint.follow(Button::Guide, chords, start);
    expect(hint.visible(start) && hint.modifier() == Button::Guide && hint.rows().size() == 4 &&
               hint.modifierGlyph() == "GUIDE",
           "a long hold shows the modifier's chords");
    const auto ended = start + std::chrono::seconds{1};
    hint.follow(std::nullopt, chords, ended);
    expect(!hint.modifier() && hint.visible(ended) && hint.rows().size() == 4,
           "its end starts the fade, the list kept while it fades");
    expect(!hint.visible(ended + std::chrono::seconds{1}), "and it is gone");
}

} // namespace

int main() try {
    rowsAreExactlyTheModifiersChords();
    rowsListTheGamesChords();
    followsTheHold();
    std::printf("chord_hint: all checks passed\n");
    return 0;
} catch (...) {
    return opensu::test::escaped();
}
