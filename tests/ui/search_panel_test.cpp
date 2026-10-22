// The search panel: the results list and its zone over the keyboard, and the layout the pointer
// hit-tests.
#include "search_panel.hpp"

#include <cstdio>

#include "check.hpp"

namespace {

using namespace opensu::ui;
using opensu::input::KeyboardStyle;
using opensu::test::expect;

void resultsZone() {
    SearchPanel panel;
    panel.open("x");
    expect(panel.keyboard().text() == "x" && panel.keyboard().kind() == EntryKind::Search,
           "the field is a search");
    expect(!panel.switchZone() && !panel.enterResults(), "no results, nowhere to go");
    panel.setResults({{"Game 0", "Steam"},
                      {"Game 1", "Steam"},
                      {"Game 2", "Steam"},
                      {"Game 3", "Steam"},
                      {"Game 4", "Steam"},
                      {"Game 5", "Steam"},
                      {"Game 6", "Steam"}});
    expect(panel.enterResults() && panel.zone() == SearchZone::Results, "the results take focus");
    for (int step = 0; step < 6; ++step) {
        panel.moveInResults(Direction::Down);
    }
    expect(panel.resultFocus() == 6 && panel.firstListed() == 2, "the list scrolls to the focus");
    expect(panel.moveInResults(Direction::Down) && panel.zone() == SearchZone::Keys,
           "down from the last result returns to the keys");
    expect(panel.switchZone() && panel.zone() == SearchZone::Results, "X switches to the results");
    expect(panel.switchZone() && panel.zone() == SearchZone::Keys, "and back");
    panel.focusResult(2);
    panel.setResults({});
    expect(panel.zone() == SearchZone::Keys, "no results leave the zone");
    panel.focusResult(0);
    expect(panel.zone() == SearchZone::Keys, "an absent result cannot be focused");
}

void layoutHitTests() {
    const Rect frame{0.0f, 0.0f, 1280.0f, 720.0f};
    for (const KeyboardStyle style : opensu::input::allKeyboardStyles) {
        const SearchLayout layout = layoutSearch(frame, 1.0f, style);
        for (std::size_t i = 0; i < searchResultRows; ++i) {
            const auto hit =
                layout.resultAt(layout.results[i].centreX(), layout.results[i].y + 1.0f);
            expect(hit && *hit == i, "a result row hits that row");
            expect(!layout.keyboard.keyAt(layout.results[i].centreX(), layout.results[i].y + 1.0f),
                   "and no key");
        }
        expect(layout.field.bottom() <= layout.results.front().y &&
                   layout.results.back().bottom() <= layout.card.bottom() &&
                   layout.card.bottom() <= layout.keyboard.panel.y,
               "the field and the results stand in the card above the docked keyboard");
        expect(layout.keyboard.preview.height == 0.0f, "the card's field replaces the preview");
        expect(layout.cardFor(0).bottom() > layout.field.bottom() &&
                   layout.cardFor(0).bottom() < layout.cardFor(1).bottom() &&
                   layout.cardFor(1).bottom() > layout.results[0].bottom() &&
                   layout.cardFor(99).height == layout.card.height &&
                   layout.cardFor(2).y == layout.card.y,
               "the card holds the rows listed, growing down from the field");
    }
}

} // namespace

int main() {
    resultsZone();
    layoutHitTests();
    std::printf("search_panel: all checks passed\n");
    return 0;
}
