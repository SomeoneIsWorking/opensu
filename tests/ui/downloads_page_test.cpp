// The Downloads page's model: focus across rows and their buttons, refresh by key, and the
// geometry the pointer and painter share.
#include "downloads_page.hpp"

#include <cstdio>
#include <string>
#include <vector>

#include "check.hpp"

namespace {

using namespace opensu::ui;
using opensu::test::expect;

DownloadsEntry entry(const char* key, std::vector<std::string> actions) {
    return DownloadsEntry{.key = key, .title = key, .actions = std::move(actions)};
}

std::vector<DownloadsEntry> three() {
    return {entry("a", {"Pause"}), entry("b", {"Download now", "Move down", "Remove"}),
            entry("c", {})};
}

void focusMovesAcrossRowsAndButtons() {
    DownloadsPage page;
    expect(!page.isOpen() && page.focused() == nullptr, "a closed page focuses nothing");
    page.open(three());
    expect(page.isOpen() && page.row() == 0 && page.action() == 0, "it opens on the first row");
    expect(!page.move(-1), "focus stops at the top");
    expect(!page.step(1), "a row with one button has nowhere to step");
    expect(page.move(1) && page.focused()->key == "b", "Down moves to the next row");
    expect(page.step(1) && page.step(1) && !page.step(1) && page.action() == 2,
           "Right walks the row's buttons and stops at the last");
    expect(page.move(1) && page.action() == 0 && !page.step(1),
           "a new row starts on its first button; a row without buttons has none");
    expect(!page.move(1), "focus stops at the bottom");
    expect(page.focusAction(1, 1) && page.row() == 1 && page.action() == 1,
           "the pointer focuses a button directly");
    expect(!page.focusAction(1, 3) && !page.focusAction(5, 0), "nothing past the ends");
    page.close();
    expect(!page.isOpen(), "it closes");
}

void refreshKeepsTheFocusedDownload() {
    DownloadsPage page;
    page.open(three());
    page.focusAction(1, 2);
    page.refresh({entry("new", {"Cancel"}), entry("a", {"Pause"}),
                  entry("b", {"Download now", "Move down", "Remove"}), entry("c", {})});
    expect(page.row() == 2 && page.action() == 2,
           "a row added above moves focus with its download");
    page.refresh(
        {entry("new", {"Cancel"}), entry("a", {"Pause"}), entry("b", {"Download now", "Remove"})});
    expect(page.row() == 2 && page.action() == 1, "a button gone clamps to the last one");
    page.refresh({entry("new", {"Cancel"}), entry("a", {"Resume", "Remove"})});
    expect(page.row() == 1 && page.action() == 0,
           "a download gone focuses the nearest row, on its first button");
    page.refresh({});
    expect(page.focused() == nullptr && !page.move(1), "an empty page focuses nothing");
}

void layoutSharesItsGeometry() {
    DownloadsPage page;
    page.open(three());
    const DownloadsLayout layout = page.layout(Rect{0.0f, 100.0f, 1280.0f, 560.0f}, 1.0f);
    expect(layout.panel.y == 100.0f && layout.panel.bottom() == 660.0f,
           "the panel fills the room between the bars");
    expect(layout.rows.size() == 3 && layout.actions[1].size() == 3 && layout.actions[2].empty(),
           "every row has its buttons");
    expect(layout.actions[1][0].x < layout.actions[1][1].x &&
               layout.actions[1][2].right() <= layout.rows[1].right(),
           "buttons sit left to right at the row's end");
    expect(layout.bodies[1].right() <= layout.actions[1][0].x &&
               layout.bars[1].y > layout.rows[1].y &&
               layout.bars[1].bottom() < layout.rows[1].bottom(),
           "the text and bar keep left of the buttons, inside the row");
    const Rect& button = layout.actions[1][2];
    const auto hit = layout.actionAt(button.centreX(), button.centreY());
    expect(hit && hit->first == 1 && hit->second == 2, "a point on a button finds it");
    expect(!layout.actionAt(layout.bodies[1].centreX(), layout.bodies[1].centreY()) &&
               layout.rowAt(layout.bodies[1].centreX(), layout.bodies[1].centreY()) == 1u,
           "a point on the text finds the row only");
    expect(!layout.rowAt(5.0f, 5.0f), "outside the panel finds nothing");
}

void focusScrollsIntoView() {
    DownloadsPage page;
    std::vector<DownloadsEntry> many;
    many.reserve(20);
    for (int i = 0; i < 20; ++i) {
        many.push_back(entry(std::to_string(i).c_str(), {"Cancel"}));
    }
    page.open(many);
    for (int i = 0; i < 19; ++i) {
        page.move(1);
    }
    const DownloadsLayout layout = page.layout(Rect{0.0f, 100.0f, 1280.0f, 560.0f}, 1.0f);
    expect(layout.rows[19].bottom() <= layout.panel.bottom() && layout.rows[19].y >= layout.panel.y,
           "the focused last row is scrolled whole into view");
    expect(layout.rows[0].bottom() < layout.panel.y, "the first rows scroll out above");
}

} // namespace

int main() {
    focusMovesAcrossRowsAndButtons();
    refreshKeepsTheFocusedDownload();
    layoutSharesItsGeometry();
    focusScrollsIntoView();
    std::printf("downloads_page: all checks passed\n");
    return 0;
}
