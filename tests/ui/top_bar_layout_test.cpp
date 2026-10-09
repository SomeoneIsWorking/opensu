// The top bar's geometry: the launchers in the status pill, and what a point hits.
#include "top_bar_layout.hpp"

#include <cstdio>

#include "check.hpp"

namespace {

using opensu::test::expect;
using opensu::test::need;
using opensu::ui::TopBarFrame;
using opensu::ui::TopBarLayout;
using opensu::ui::TopBarMetrics;

TopBarLayout layoutOf(std::size_t launchers, float content = 120.0f) {
    const float dp = 1.5f;
    const TopBarMetrics metrics{1280.0f / dp, 800.0f / dp};
    return opensu::ui::layoutTopBar(metrics,
                                    TopBarFrame{1280.0f, 800.0f, dp, false, content, launchers});
}

void pillFollowsItsContent() {
    const TopBarLayout clockOnly = layoutOf(0, 70.0f);
    const TopBarLayout withBattery = layoutOf(0, 190.0f);
    expect(clockOnly.status.width < withBattery.status.width, "no battery, a shorter pill");
    expect(clockOnly.status.right() == withBattery.status.right(), "still right-aligned");
    const float padding = clockOnly.pill.paddingHorizontal * 1.5f;
    expect(clockOnly.status.width == 70.0f + 2.0f * padding,
           "the pill is the content plus its padding at each end");
    const TopBarLayout launchers = layoutOf(2, 70.0f);
    expect(launchers.status.width == launchers.launcherColumn + 70.0f + 2.0f * padding,
           "the launcher column comes first");
}

void launchersInThePill() {
    const TopBarLayout none = layoutOf(0);
    expect(none.launchers.empty() && none.launcherColumn == 0.0f, "no launcher, no column");
    const TopBarLayout three = layoutOf(3);
    expect(three.launchers.size() == 3, "a cell per launcher");
    expect(three.status.width > none.status.width, "the pill grows for them");
    expect(three.status.right() == none.status.right(), "towards the left, its end fixed");
    for (const opensu::ui::Rect& cell : three.launchers) {
        expect(cell.x >= three.status.x && cell.right() <= three.status.x + three.launcherColumn,
               "each sits in the launcher column");
        expect(cell.y >= three.status.y && cell.bottom() <= three.status.bottom(),
               "and inside the pill's height");
    }
    expect(three.launchers[0].right() < three.launchers[1].x, "badges do not overlap");
}

void hitTesting() {
    const TopBarLayout layout = layoutOf(3);
    for (std::size_t i = 0; i < layout.launchers.size(); ++i) {
        const opensu::ui::Rect& cell = layout.launchers[i];
        expect(need(layout.launcherAt(cell.centreX(), cell.centreY()), "centre") == i,
               "a badge's centre hits it");
    }
    const opensu::ui::Rect& first = layout.launchers[0];
    const opensu::ui::Rect& second = layout.launchers[1];
    expect(layout.launcherAt(first.right() + 1.0f, first.centreY()) == 0u,
           "a hair past the circle still hits it");
    expect(!layout.launcherAt((first.right() + second.x) * 0.5f, first.centreY()),
           "the middle of the gap hits none");
    expect(!layout.launcherAt(layout.status.right() - 2.0f, layout.status.centreY()),
           "the clock's area hits none");
    expect(!layoutOf(0).launcherAt(layout.launchers[0].centreX(), layout.launchers[0].centreY()),
           "no launcher shown, none hit");
}

} // namespace

int main() {
    launchersInThePill();
    pillFollowsItsContent();
    hitTesting();
    std::printf("top_bar_layout: all checks passed\n");
    return 0;
}
