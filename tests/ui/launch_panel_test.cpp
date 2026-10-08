// The launch panel's state.
#include "launch_panel.hpp"

#include <cstdio>

#include "check.hpp"

namespace {

using iideck::test::expect;
using iideck::ui::LaunchPanel;

void opensAtStarting() {
    LaunchPanel panel;
    expect(!panel.isOpen(), "starts closed");
    panel.open("Bloons TD 6");
    expect(panel.isOpen(), "opens");
    expect(panel.title() == "Bloons TD 6", "keeps the game's title");
    expect(panel.line() == "Starting" && !panel.fraction(), "starts at Starting, unmeasured");
}

void updatesAndClamps() {
    LaunchPanel panel;
    panel.open("Bloons TD 6");
    panel.update("Updating · 42%", 0.42);
    expect(panel.line() == "Updating · 42%" && panel.fraction() == 0.42, "takes the stage");
    panel.update("Updating", 1.5);
    expect(panel.fraction() == 1.0, "clamps the measure");
    panel.update("Loading", std::nullopt);
    expect(!panel.fraction(), "a stage without a measure drops it");
    panel.open("Celeste");
    expect(panel.line() == "Starting" && !panel.fraction(), "a new launch starts over");
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
