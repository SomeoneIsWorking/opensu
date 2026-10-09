// The dock's geometry against navigation.md §1.2's worked numbers, and the formulas at other sizes.
#include "dock_metrics.hpp"

#include <cstdio>

#include "check.hpp"
#include "top_bar_metrics.hpp"

namespace {

using iideck::test::expect;
using iideck::test::near;
using iideck::ui::DockMetrics;
using iideck::ui::plainItem;
using iideck::ui::wideItem;

/// iiSU's five items: Home, ROMs (wide), RetroAchievements, Friends, Apps.
std::vector<float> fiveItems() {
    return std::vector<float>{plainItem, wideItem, plainItem, plainItem, plainItem};
}
/// iideck's two: Home and Library.
std::vector<float> twoItems() {
    return std::vector<float>{plainItem, wideItem};
}

void referenceCapture() {
    const DockMetrics dock{853.0f, 456.0f, fiveItems()};
    near(dock.height, 50.94f, "bar height at 853 x 456 dp", 0.01);
    near(dock.iconSize, 43.81f, "icon box", 0.01);
    near(dock.itemWidths[1], 54.10f, "the ROMs item is 1.235 icons wide", 0.01);
    near(dock.itemWidths[0], 43.81f, "the others are one icon wide", 0.01);
    near(dock.spacing, 1.53f, "spacing", 0.01);
    near(dock.horizontalPadding, 6.11f, "side padding", 0.01);
    near(dock.verticalPadding, 3.06f, "vertical padding", 0.01);
    near(dock.iconPadding, 4.38f, "icon inset", 0.01);
    near(dock.width, 247.7f, "five items make a bar 247.7 dp wide", 0.1);
    near(dock.borderWidth, 1.91f, "glass border", 0.01);
    near(dock.badgeSize, 21.4f, "badge", 0.01);
    near(dock.badgeOutset, 4.08f, "badge outset", 0.01);
    near(dock.badgeRise, 5.60f, "badge rise", 0.01);
}

void ourTwoItems() {
    const DockMetrics dock{853.0f, 456.0f, twoItems()};
    near(dock.height, 50.94f, "the height does not depend on the item count", 0.01);
    // (1 + 1.235) icons, one gap, two side paddings.
    near(dock.width, (1.0f + 1.235f) * 43.81f + 1.53f + 2.0f * 6.11f, "two items make a bar", 0.05);
    expect(dock.itemWidths.size() == 2, "an item width each");
}

void otherSize() {
    // 480 x 270 dp: f0 floors at 0.68, so s = 0.82 and the prompt height 42.24 caps the bar.
    const DockMetrics dock{480.0f, 270.0f, fiveItems()};
    near(dock.height, 42.24f, "the bar is capped by the prompt row's height", 0.01);
    near(dock.iconSize, 36.33f, "icon box", 0.01);
    near(dock.spacing, 1.267f, "spacing", 0.01);
    near(dock.horizontalPadding, 5.069f, "side padding", 0.01);
    near(dock.verticalPadding, 3.0f, "vertical padding floors at 3", 0.001);
    near(dock.iconPadding, 4.0f, "icon inset floors at 4", 0.001);
    near(dock.borderWidth, 1.6f, "the border floors at 1.6", 0.001);
    near(dock.badgeSize, 20.0f, "the badge floors at 20", 0.001);
    near(dock.badgeOutset, 3.38f, "badge outset", 0.01);
    near(dock.badgeRise, 4.65f, "badge rise", 0.01);

    const DockMetrics large{1920.0f, 1080.0f, twoItems()};
    near(large.height, 50.94f + 0.0f * 0.0f, "a larger screen does not grow the bar past f0 = 1",
         10.0);
    expect(large.height <= 60.0f && large.iconSize <= 58.0f, "the bar never exceeds its clamps");
}

void tooWide() {
    // Twelve items cannot fit 240 dp; the set is refitted smaller until it does or hits its floor.
    const std::vector<float> many(12, plainItem);
    const DockMetrics narrow{240.0f, 480.0f, many};
    const DockMetrics roomy{853.0f, 480.0f, many};
    expect(narrow.height < roomy.height, "a bar that overflows shrinks");
    expect(narrow.width < roomy.width, "and so is narrower");
}

void insetLeavesRoom() {
    // iiSU's grid bottom inset is its prompt row's, and the dock (6 dp below, 8 dp above) fits in
    // it.
    for (const auto [w, h] : {std::pair{853.0f, 456.0f}, std::pair{853.0f, 480.0f},
                              std::pair{480.0f, 270.0f}, std::pair{1280.0f, 800.0f}}) {
        const DockMetrics dock{w, h, twoItems()};
        const iideck::ui::TopBarMetrics top{w, h};
        expect(dock.height + DockMetrics::bottomGap + DockMetrics::topGap <= top.gridBottomInset(),
               "the grid's bottom inset leaves room for the dock");
    }
}

void placement() {
    const DockMetrics dock{853.0f, 456.0f, twoItems()};
    const float dp = 2.0f;
    const iideck::ui::DockLayout layout =
        iideck::ui::layoutDock(dock, 853.0f * dp, 456.0f * dp, dp);
    near(layout.bar.centreX(), 853.0f, "the bar is centred", 0.01);
    near(layout.bar.bottom(), (456.0f - 6.0f) * dp, "6 dp above the bottom edge", 0.01);
    near(layout.bar.height, dock.height * dp, "its height", 0.01);
    near(layout.items[0].x, layout.bar.x + dock.horizontalPadding * dp,
         "items start inside the padding", 0.01);
    near(layout.items[1].x, layout.items[0].right() + dock.spacing * dp,
         "one spacing between items", 0.01);
    near(layout.items[1].right() + dock.horizontalPadding * dp, layout.bar.right(),
         "and end inside the padding", 0.01);
    near(layout.items[0].centreY(), layout.bar.centreY(), "items are centred vertically", 0.01);
    near(layout.icons[0].x, layout.items[0].x + (dock.iconPadding - 2.0f) * dp,
         "an icon is inset and nudged 2 dp left", 0.01);
    near(layout.icons[0].width, (dock.iconSize - 2.0f * dock.iconPadding) * dp, "icon width", 0.01);
    near(layout.leftBadge.x, layout.bar.x - dock.badgeOutset * dp,
         "LB pokes out of the left corner", 0.01);
    near(layout.leftBadge.y, layout.bar.y - dock.badgeRise * dp, "above the top", 0.01);
    near(layout.rightBadge.right(), layout.bar.right() + dock.badgeOutset * dp,
         "RB pokes out of the right corner", 0.01);
    near(layout.rightBadge.width, dock.badgeSize * dp, "badge size", 0.01);
}

void pointerHits() {
    const DockMetrics dock{853.0f, 456.0f, twoItems()};
    const float dp = 2.0f;
    const iideck::ui::DockLayout layout =
        iideck::ui::layoutDock(dock, 853.0f * dp, 456.0f * dp, dp);
    const auto at = [&](float x, float y, float slide = 0.0f) {
        return iideck::ui::dockItemAt(layout, slide, x, y);
    };
    const float y = layout.bar.centreY();
    expect(at(layout.items[0].centreX(), y) == 0, "the middle of Home's item is Home");
    expect(at(layout.items[1].centreX(), y) == 1, "the middle of Library's item is Library");
    expect(at(layout.items[1].right() - 1.0f, y) == 1, "an item reaches its right edge");
    expect(at(layout.items[0].centreX(), layout.bar.y + 1.0f) == 0,
           "an item reaches the bar's full height, not only its icon's");
    expect(!at(layout.items[0].centreX(), layout.bar.y - 1.0f), "above the bar is no item");
    expect(!at(layout.items[0].centreX(), layout.bar.bottom() + 1.0f), "below the bar is no item");
    expect(!at(layout.bar.x + 1.0f, y), "the bar's padding is no item");
    expect(!at((layout.items[0].right() + layout.items[1].x) * 0.5f, y) ||
               layout.items[1].x - layout.items[0].right() < 1.0f,
           "the gap between items is none");
    expect(!at(layout.items[0].centreX(), y, 100.0f) &&
               at(layout.items[0].centreX(), y + 100.0f, 100.0f) == 0,
           "a bar slid down is hit where it is drawn");
    expect(iideck::ui::onRestingDock(layout, layout.bar.centreX(), y) &&
               !iideck::ui::onRestingDock(layout, layout.bar.x - 1.0f, y),
           "the resting bar is where a pointer asks for a hidden dock");
}

} // namespace

int main() {
    referenceCapture();
    ourTwoItems();
    otherSize();
    tooWide();
    insetLeavesRoom();
    placement();
    pointerHits();
    std::printf("dock_metrics: all checks passed\n");
    return 0;
}
