// The breadcrumb trail on the Settings screen and the Devices page: the page then its category or
// tab, in place of the section's levels, and the page's level is the only place.
#include <cstdio>

#include "breadcrumb_trail.hpp"
#include "ui/check.hpp"

namespace {

using namespace opensu;
using test::expect;

void settingsReplacesTheSectionTrail() {
    app::TrailState state;
    state.section = library::Section::Library;
    state.search = "hades";
    state.settings = "Audio";
    const ui::Trail trail = app::trailOf(state);
    expect(trail.size() == 2 && trail[0].label == "Settings" && trail[1].label == "Audio",
           "Settings > Audio, nothing of Library");
    expect(ui::isPlace(trail[0]) && !ui::isPlace(trail[1]),
           "the Settings level can be clicked, the category is a page");
    state.settings.clear();
    expect(app::trailOf(state)[0].label == "Library",
           "without the screen the section's trail is back");
}

void devicesReplacesTheSectionTrail() {
    app::TrailState state;
    state.section = library::Section::Library;
    state.folder = library::Folder{library::AllGames{}};
    state.devices = "Bluetooth";
    ui::Trail trail = app::trailOf(state);
    expect(trail.size() == 2 && trail[0].label == "Devices" && trail[1].label == "Bluetooth",
           "Devices > Bluetooth, nothing of Library");
    expect(ui::isPlace(trail[0]) && trail[0].kind == ui::CrumbKind::Devices &&
               !ui::isPlace(trail[1]),
           "the Devices level can be clicked, the tab is a page");
    state.settings = "Audio";
    trail = app::trailOf(state);
    expect(trail[0].label == "Devices", "when both are named the Devices page, opened later, wins");
    state.devices.clear();
    state.settings.clear();
    expect(app::trailOf(state)[0].label == "Library", "without the page the section's trail is back");
}

} // namespace

int main() {
    devicesReplacesTheSectionTrail();
    settingsReplacesTheSectionTrail();
    std::printf("breadcrumb_trail: all checks passed\n");
    return 0;
}
