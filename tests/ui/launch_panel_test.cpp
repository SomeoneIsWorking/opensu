// The launch panel's state.
#include "launch_panel.hpp"

#include <cstdio>

#include "check.hpp"

namespace {

using opensu::test::expect;
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

} // namespace

int main() {
    opensAtStarting();
    updatesAndClamps();
    std::printf("launch panel: all checks passed\n");
    return 0;
}
