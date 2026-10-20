// The first-run integration chooser: focus over the options and Done, a switch flipped by A, and a
// layout that keeps the card inside the smallest window with nothing overlapping.
#include "integration_chooser.hpp"

#include <cstdio>
#include <vector>

#include "check.hpp"

namespace {

using namespace opensu::ui;
using opensu::test::expect;

std::vector<IntegrationOption> five() {
    std::vector<IntegrationOption> options;
    for (const char* id : {"steam", "epic", "gog", "emulators", "plugins"}) {
        options.push_back(IntegrationOption{id, id, "what it does", false});
    }
    return options;
}

void focusAndSwitches() {
    IntegrationChooser chooser;
    chooser.open(five());
    expect(chooser.isOpen() && chooser.focus() == 0, "opens on the first option");
    expect(!chooser.move(-1), "nothing above it");
    expect(chooser.toggleFocused() && chooser.options()[0].on, "A turns it on");
    expect(chooser.toggleFocused() && !chooser.options()[0].on, "and off");
    for (int i = 0; i < 5; ++i) {
        chooser.move(1);
    }
    expect(chooser.onDone() && !chooser.move(1), "Done is last");
    expect(!chooser.toggleFocused(), "Done has no switch");
    expect(chooser.focusItem(2) && !chooser.focusItem(2) && !chooser.focusItem(9),
           "a pointer focuses an item once, and no other");
    expect(chooser.rows().size() == 5 && chooser.rows()[2].kind == RowKind::Toggle,
           "the options are switch rows");
}

void fitsTheSmallestWindow() {
    // 960 x 600 at that window's dp, and a frame of the reference height.
    for (const PanelFrame frame :
         {PanelFrame{960.0f, 600.0f, 1.25f}, PanelFrame{853.0f, 480.0f, 1.0f},
          PanelFrame{1280.0f, 800.0f, 1.5f}}) {
        const IntegrationChooserLayout layout =
            layoutIntegrationChooser(frame, 30.0f * frame.dp, 40.0f * frame.dp, 5);
        expect(layout.card.y >= 0.0f && layout.card.bottom() <= frame.height &&
                   layout.card.x >= 0.0f && layout.card.right() <= frame.width,
               "the card stays inside the frame");
        expect(layout.rows.size() == 5, "one box per option");
        float last = layout.intro.bottom();
        for (const SettingsRowBox& row : layout.rows) {
            expect(row.rect.y >= last && row.rect.height > 0.0f, "rows follow without overlap");
            expect(row.control.x > row.rect.x && row.control.right() <= row.rect.right(),
                   "each switch sits inside its row");
            last = row.rect.bottom();
        }
        expect(layout.done.y >= last && layout.done.bottom() <= layout.card.bottom(),
               "Done is under the rows, inside the card");
        expect(layout.itemAt(layout.rows[1].rect.centreX(), layout.rows[1].rect.centreY()) == 1 &&
                   layout.itemAt(layout.done.centreX(), layout.done.centreY()) == 5 &&
                   !layout.itemAt(0.0f, 0.0f),
               "the pointer finds options and Done");
    }
}

} // namespace

int main() {
    focusAndSwitches();
    fitsTheSmallestWindow();
    std::printf("integration_chooser: all checks passed\n");
    return 0;
}
