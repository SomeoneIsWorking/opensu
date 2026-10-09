// The launch panel's state.
#include "launch_panel.hpp"

#include <cstdio>

#include "check.hpp"

namespace {

using opensu::test::expect;
using opensu::test::near;
using opensu::ui::LaunchPanel;

void opensAtStarting() {
    LaunchPanel panel;
    expect(!panel.isOpen(), "starts closed");
    panel.open("Bloons TD 6");
    expect(panel.isOpen(), "opens");
    expect(panel.title() == "Bloons TD 6", "keeps the game's title");
    expect(panel.line() == "Starting" && !panel.fraction(), "starts at Starting, unmeasured");
    expect(panel.hints() == std::vector<opensu::ui::PanelHint>{{"B", "Cancel"}}, "B cancels");
}

void updatesAndClamps() {
    LaunchPanel panel;
    panel.open("Bloons TD 6");
    panel.update("Updating · 42%", 0.42);
    expect(panel.line() == "Updating · 42%" && panel.fraction() == 0.42, "takes the stage");
    panel.update("Updating", 1.5);
    expect(panel.fraction() == 1.0, "clamps the measure");
    panel.update("Loading", std::nullopt);
    expect(!panel.fraction() && panel.busy(), "a stage without a measure drops it");
    panel.update("Not installed", std::nullopt, false);
    expect(!panel.busy(), "a stage waiting on the player is not busy");
    panel.setHints({{"A", "Install"}, {"B", "Cancel"}});
    expect(panel.hints().size() == 2, "takes the buttons it offers");
    panel.open("Celeste");
    expect(panel.line() == "Starting" && !panel.fraction() && panel.hints().size() == 1,
           "a new launch starts over");
    panel.close();
    expect(!panel.isOpen(), "closes");
}

void hintsUnderThePointer() {
    const opensu::ui::PanelLayout layout = opensu::ui::layoutPanel(
        1920.0f, 1080.0f, 2.25f, opensu::ui::PanelMetrics{54.0f, 36.0f, {120.0f, 200.0f}});
    near(layout.card.centreX(), 960.0, "the card is centred");
    near(layout.card.centreY(), 540.0, "both ways");
    expect(layout.hints.size() == 2, "a rectangle per hint");
    near(layout.hints[0].width, 120.0, "as wide as it was measured");
    near(layout.hints[1].x - layout.hints[0].right(), 45.0, "20 dp apart");
    near((layout.hints[0].x + layout.hints[1].right()) * 0.5f, 960.0, "the row is centred");
    for (std::size_t i = 0; i < layout.hints.size(); ++i) {
        const auto hit = layout.hintAt(layout.hints[i].centreX(), layout.hints[i].centreY());
        expect(hit && *hit == i, "a hint's centre hits it");
    }
    const float between = layout.hints[0].right() + 5.0f;
    const auto beside = layout.hintAt(between, layout.hintY);
    expect(beside && *beside == 0, "the room beside a hint counts for it");
    expect(!layout.hintAt(layout.hints[0].centreX(), layout.card.y + 5.0f), "the title hits none");
    expect(!layout.hintAt(layout.hints[0].x - 60.0f, layout.hintY), "nor does the far margin");
}

} // namespace

int main() {
    hintsUnderThePointer();
    opensAtStarting();
    updatesAndClamps();
    std::printf("launch panel: all checks passed\n");
    return 0;
}
