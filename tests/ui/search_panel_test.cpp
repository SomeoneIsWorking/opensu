// The search panel: the field, the drawn keyboard's D-pad walk, the results list and the layout
// the pointer hit-tests.
#include "search_panel.hpp"

#include <cstdio>

#include "check.hpp"

namespace {

using namespace opensu::ui;
using opensu::test::expect;

std::size_t indexOf(KeyKind kind, char character = '\0') {
    const auto keys = searchKeys();
    for (std::size_t i = 0; i < keys.size(); ++i) {
        if (keys[i].kind == kind &&
            (kind != KeyKind::Character || keys[i].character == character)) {
            return i;
        }
    }
    expect(false, "key exists");
    return 0;
}

SearchPress pressKey(SearchPanel& panel, KeyKind kind, char character = '\0') {
    panel.focusKey(indexOf(kind, character));
    return panel.press();
}

void keysTypeIntoTheField() {
    SearchPanel panel;
    panel.open("");
    expect(panel.isOpen() && panel.text().empty(), "opens empty");
    expect(pressKey(panel, KeyKind::Character, 'h').edited && panel.text() == "h",
           "a letter types");
    pressKey(panel, KeyKind::Character, 'i');
    expect(!pressKey(panel, KeyKind::Space).edited || panel.text() == "hi ", "space types");
    expect(!pressKey(panel, KeyKind::Space).edited, "no double space");
    pressKey(panel, KeyKind::Character, 'x');
    expect(panel.text() == "hi x", "text accumulates");
    expect(pressKey(panel, KeyKind::Backspace).edited && panel.text() == "hi ",
           "delete removes one");
    expect(pressKey(panel, KeyKind::Clear).edited && panel.text().empty(), "clear empties");
    expect(!pressKey(panel, KeyKind::Clear).edited, "clearing nothing is no edit");
    expect(pressKey(panel, KeyKind::Done).close, "done closes");
}

void physicalKeyboardTextAndLimits() {
    SearchPanel panel;
    panel.open("ab");
    expect(panel.type("c\xC3\xA9") && panel.text() == "abc\xC3\xA9", "typed text appends");
    expect(panel.backspace() && panel.text() == "abc", "backspace removes a whole character");
    expect(!panel.type(""), "empty text is no edit");
    panel.clear();
    panel.type(std::string(searchMostLength, 'a'));
    expect(!panel.type("b") && panel.text().size() == searchMostLength, "the field is bounded");
    panel.clear();
    expect(!panel.backspace(), "backspace on nothing");
}

void dpadWalksTheKeys() {
    SearchPanel panel;
    panel.open("");
    expect(panel.keyFocus() == 0, "starts on the first key");
    expect(!panel.move(Direction::Left) && !panel.move(Direction::Up), "edges stop the walk");
    expect(panel.move(Direction::Right) && panel.keyFocus() == 1, "right steps");
    expect(panel.move(Direction::Down) && searchKeys()[panel.keyFocus()].row == 1, "down a row");
    for (int i = 0; i < 3; ++i) {
        panel.move(Direction::Down);
    }
    expect(searchKeys()[panel.keyFocus()].row == 4, "the bottom row");
    expect(!panel.move(Direction::Down), "no row below the last");
    const std::size_t space = indexOf(KeyKind::Space);
    panel.focusKey(space);
    panel.move(Direction::Up);
    const std::size_t above = panel.keyFocus();
    panel.move(Direction::Down);
    expect(panel.keyFocus() == space || searchKeys()[panel.keyFocus()].row == 4,
           "down returns to the bottom row");
    expect(above < space, "up goes to an earlier key");
}

void resultsZone() {
    SearchPanel panel;
    panel.open("x");
    expect(!panel.move(Direction::Up) && !panel.switchZone(), "no results, nowhere to go");
    panel.setResults({{"Game 0", "Steam"},
                      {"Game 1", "Steam"},
                      {"Game 2", "Steam"},
                      {"Game 3", "Steam"},
                      {"Game 4", "Steam"}});
    expect(panel.move(Direction::Up) && panel.zone() == SearchZone::Results,
           "up from the top key row enters the results");
    expect(panel.press().open == std::optional<std::size_t>{0}, "A opens the focused result");
    panel.move(Direction::Down);
    panel.move(Direction::Down);
    panel.move(Direction::Down);
    expect(panel.resultFocus() == 3 && panel.firstListed() == 1, "the list scrolls to the focus");
    panel.move(Direction::Down);
    expect(panel.move(Direction::Down) && panel.zone() == SearchZone::Keys,
           "down from the last result returns to the keys");
    expect(panel.switchZone() && panel.zone() == SearchZone::Results, "X switches to the results");
    expect(panel.switchZone() && panel.zone() == SearchZone::Keys, "and back");
    panel.setResults({});
    expect(panel.zone() == SearchZone::Keys, "no results leave the zone");
    panel.focusResult(0);
    expect(panel.zone() == SearchZone::Keys, "an absent result cannot be focused");
}

void layoutHitTests() {
    const Rect frame{0.0f, 0.0f, 1280.0f, 720.0f};
    const SearchLayout layout = layoutSearch(frame, 1.0f);
    expect(layout.keys.size() == searchKeys().size(), "a rectangle per key");
    expect(layout.panel.x >= 0.0f && layout.panel.right() <= frame.width &&
               layout.panel.y >= 0.0f && layout.panel.bottom() <= frame.height,
           "the panel fits the frame");
    for (std::size_t i = 0; i < layout.keys.size(); ++i) {
        const auto hit = layout.keyAt(layout.keys[i].centreX(), layout.keys[i].y + 1.0f);
        expect(hit && *hit == i, "a key's centre hits that key");
    }
    expect(!layout.keyAt(1.0f, 1.0f), "outside hits nothing");
    for (std::size_t i = 0; i < searchResultRows; ++i) {
        const auto hit = layout.resultAt(layout.results[i].centreX(), layout.results[i].y + 1.0f);
        expect(hit && *hit == i, "a result row hits that row");
    }
    const SearchLayout narrow = layoutSearch(Rect{0.0f, 0.0f, 400.0f, 300.0f}, 1.0f);
    expect(narrow.panel.width <= 400.0f, "the panel never exceeds a narrow frame");
}

} // namespace

int main() {
    keysTypeIntoTheField();
    physicalKeyboardTextAndLimits();
    dpadWalksTheKeys();
    resultsZone();
    layoutHitTests();
    std::printf("search_panel: all checks passed\n");
    return 0;
}
