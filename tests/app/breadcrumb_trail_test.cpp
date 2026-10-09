// The breadcrumb trail on the Settings screen: "Settings" then the category, in place of the
// section's levels, and the Settings level is the only place.
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

} // namespace

int main() {
    settingsReplacesTheSectionTrail();
    std::printf("breadcrumb_trail: all checks passed\n");
    return 0;
}
