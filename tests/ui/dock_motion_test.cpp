// The dock's slide and the selected icon's pop against navigation.md §1.4, §1.6 and §1.7.
#include "dock_motion.hpp"

#include <cstdio>

#include "check.hpp"
#include "tile_motion.hpp"

namespace {

using opensu::test::expect;
using opensu::test::near;
using opensu::ui::DockVisibility;
using opensu::ui::IconPop;

void easing() {
    near(opensu::ui::motion::fastOutSlowIn(0.0), 0.0, "FastOutSlowIn starts at 0");
    near(opensu::ui::motion::fastOutSlowIn(1.0), 1.0, "and ends at 1");
    near(opensu::ui::motion::fastOutSlowIn(0.5), 0.7756, "the curve is 0.776 at half way", 1e-3);
}

void pinRule() {
    using opensu::library::Section;
    expect(opensu::ui::dockPinned(Section::Home, false), "Home always keeps the dock");
    expect(opensu::ui::dockPinned(Section::Library, true), "a pinned Library keeps the dock");
    expect(!opensu::ui::dockPinned(Section::Library, false), "an unpinned Library hides it");
}

void pinnedDock() {
    DockVisibility dock;
    near(dock.progress(0.0), 1.0, "the dock starts in, as it is on Home");
    dock.setPinned(true, 100.0);
    near(dock.progress(5000.0), 1.0, "a pinned dock stays");
}

void revealedDock() {
    using opensu::ui::motion::fastOutLinearIn;
    using opensu::ui::motion::linearOutSlowIn;
    DockVisibility dock;
    dock.setPinned(false, 0.0);
    near(dock.progress(0.0), 1.0, "leaving Home does not snap the dock away");
    near(dock.progress(125.0), 0.0, "it has slid out 125 ms later");
    near(dock.progress(5000.0), 0.0, "and stays out");

    dock.reveal(10000.0);
    near(dock.progress(10000.0), 0.0, "a reveal starts from out");
    near(dock.progress(10085.0), linearOutSlowIn(0.5), "half way through the ease-out", 1e-3);
    near(dock.progress(10170.0), 1.0, "in after 170 ms");
    near(dock.progress(11199.0), 1.0, "held in for 1200 ms from the press");
    near(dock.progress(11200.0 + 62.5), 1.0 - fastOutLinearIn(0.5),
         "then out, from the moment the reveal ran out", 1e-3);
    near(dock.progress(11200.0 + 125.0), 0.0, "gone 125 ms after that");
}

void slideShapes() {
    // navigation.md §5.1, light recording at 60 fps: the dock comes back up fast and settles
    // slowly, and leaves slowly and quickly at the end.
    DockVisibility dock;
    dock.setPinned(false, 0.0);
    dock.reveal(1000.0);
    expect(dock.progress(1000.0 + 1000.0 / 60.0) > 0.25f, "a frame into the show it is a third in");
    expect(dock.progress(1000.0 + 5000.0 / 60.0) > 0.8f,
           "five frames in it is most of the way home");
    DockVisibility leaving;
    leaving.setPinned(false, 0.0);
    expect(leaving.progress(2000.0 / 60.0) > 0.8f, "two frames into the hide it has barely moved");
    expect(leaving.progress(5000.0 / 60.0) < 0.5f, "five frames in it is more than half gone");
}

void pressesExtendTheReveal() {
    DockVisibility dock;
    dock.setPinned(false, 0.0);
    dock.reveal(1000.0);
    dock.reveal(1900.0);
    near(dock.progress(3000.0), 1.0, "a second press holds it 1200 ms from itself");
    near(dock.progress(3100.0 + 125.0), 0.0, "and it goes after that");
}

void interruptedSlide() {
    DockVisibility dock;
    dock.setPinned(false, 0.0);
    // Slid out by 125 ms; a press at 1000 begins sliding in, and one more at 1060 keeps going.
    dock.reveal(1000.0);
    const float partway = dock.progress(1060.0);
    expect(partway > 0.0f && partway < 1.0f, "mid-slide");
    dock.setPinned(true, 1060.0);
    near(dock.progress(1060.0), partway, "pinning mid-slide continues from where the dock is",
         1e-4);
    near(dock.progress(1400.0), 1.0, "and arrives");
    dock.setPinned(false, 5000.0);
    near(dock.progress(5000.0), 1.0, "unpinning with no reveal left starts a slide out from in");
    near(dock.progress(5125.0), 0.0, "which takes 125 ms");
}

void iconPop() {
    IconPop home{true};
    near(home.scale(0.0), 1.04, "an item selected at first sits at 1.04");
    IconPop library{false};
    near(library.scale(0.0), 1.0, "an unselected one at 1");

    library.select(true, 1000.0);
    near(library.scale(1000.0), 1.0, "selecting starts from 1");
    near(library.scale(1120.0), 1.06, "peaks at 1.06 after 120 ms");
    near(library.scale(1230.0), 1.04, "settles at 1.04 110 ms later");
    near(library.scale(9000.0), 1.04, "and holds");
    const float mid = library.scale(1060.0);
    expect(mid > 1.0f && mid < 1.06f, "rising in between");

    library.select(true, 2000.0);
    near(library.scale(2000.0), 1.04, "selecting again changes nothing");

    library.select(false, 3000.0);
    near(library.scale(3000.0), 1.04, "deselecting starts from where it is");
    near(library.scale(3090.0), 1.0, "and is at 1 after 90 ms");
    near(library.scale(4000.0), 1.0, "holding");

    IconPop interrupted{false};
    interrupted.select(true, 0.0);
    interrupted.select(false, 60.0);
    const float from = interrupted.scale(60.0);
    expect(from > 1.0f && from < 1.06f, "a deselect mid-pop starts from the mid-pop scale");
    near(interrupted.scale(150.0), 1.0, "and still takes 90 ms");
}

} // namespace

int main() {
    easing();
    pinRule();
    pinnedDock();
    revealedDock();
    slideShapes();
    pressesExtendTheReveal();
    interruptedSlide();
    iconPop();
    std::printf("dock_motion: all checks passed\n");
    return 0;
}
