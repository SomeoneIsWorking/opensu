// HomeLayout against iiSU's grid numbers (docs/reference/iisu/home-grid.md §1).
#include "home_layout.hpp"

#include <cstdio>

#include "check.hpp"

namespace {

using iideck::test::expect;
using iideck::test::near;
using iideck::ui::HomeLayout;
using iideck::ui::HomeLayoutInput;
using iideck::ui::ScrollMode;

/// A 1280 x 800 window at 1.5 px per dp with a 60 px top bar and 40 px prompt row.
HomeLayoutInput window(std::size_t items, ScrollMode mode) {
    return HomeLayoutInput{.width = 1280.0f,
                           .height = 800.0f,
                           .dp = 1.5f,
                           .items = items,
                           .mode = mode,
                           .topInset = 60.0f,
                           .bottomInset = 40.0f};
}

void defaultViewport() {
    const HomeLayout layout{window(12, ScrollMode::Flow)};
    expect(layout.rows() == 3, "the default viewport has 3 rows");
    expect(layout.columns() == 4, "12 items in 3 rows make 4 columns");
    near(layout.cellWidth(), layout.cellHeight(), "cells are square");
    expect(HomeLayout::clampRows(0) == 1, "rows are at least 1");
    expect(HomeLayout::clampRows(9) == 6, "rows are at most 6");
    expect(HomeLayout::clampColumns(0) == 1, "horizontal columns are at least 1");
}

void gapRule() {
    near(HomeLayout::gapForCell(50.0f), 10.0f, "a small cell takes the 10 px minimum gap");
    near(HomeLayout::gapForCell(100.0f), 12.0f, "the gap is 12% of the cell");
    near(HomeLayout::gapForCell(200.0f), 24.0f, "the gap is 12% of the cell");
    near(HomeLayout::gapForCell(300.0f), 32.0f, "a large cell takes the 32 px maximum gap");
}

void flowMetrics() {
    const HomeLayout layout{window(39, ScrollMode::Flow)};
    // Hand-computed from hx2.g: inset 12, padding 12.8, content 674.4, gap refined twice.
    near(layout.viewportX(), 12.0f, "the flow inset is 8 dp");
    near(layout.gap(), 24.91008f, "gap after two refinements");
    near(layout.cellHeight(), 208.19328f, "cell height");
    near(layout.paddingTop(), 72.8f, "top padding is base padding plus the top inset");
    near(layout.paddingLeft(), 72.8f, "flow left padding is the top padding");
    near(layout.maxScroll(), 2346.13696f, "flow scrolls until the last column is centred", 1e-2);
}

void columnMajor() {
    const HomeLayout layout{window(12, ScrollMode::Flow)};
    const auto cell = [&layout](std::size_t index, int column, int row) {
        const iideck::ui::GridCell got = layout.cellOf(index);
        return got.left == column && got.top == row && got.page == 0;
    };
    expect(cell(0, 0, 0) && cell(1, 0, 1) && cell(2, 0, 2) && cell(3, 1, 0) && cell(11, 3, 2),
           "items fill top to bottom, then the next column");
    const float pitch = layout.cellWidth() + layout.gap();
    near(layout.contentRect(3).x - layout.contentRect(0).x, pitch,
         "next column is one pitch right");
    near(layout.contentRect(1).y - layout.contentRect(0).y, layout.cellHeight() + layout.gap(),
         "next row is one pitch down");
}

void flowScrollKeepsFocusVisible() {
    const HomeLayout layout{window(39, ScrollMode::Flow)};
    const float viewport = layout.viewportWidth();
    float target = 0.0f;
    const auto visible = [&](std::size_t index) {
        const iideck::ui::Rect rect = layout.contentRect(index);
        return rect.x >= target - 0.01f && rect.right() <= target + viewport + 0.01f;
    };
    for (std::size_t column = 1; column < 13; ++column) {
        target = layout.scrollTarget(column * 3, target, 1);
        expect(visible(column * 3), "moving right keeps the focused column in view");
        expect(target >= 0.0f && target <= layout.maxScroll(), "target stays in range");
    }
    for (std::size_t column = 12; column-- > 0;) {
        target = layout.scrollTarget(column * 3, target, -1);
        expect(visible(column * 3), "moving left keeps the focused column in view");
    }
    near(target, 0.0f, "the first column scrolls back to the start");
    // A step right from the start leads by clamp(1.85 pitch, 0.12 vp, 0.32 vp) before moving.
    expect(layout.scrollTarget(3, 0.0f, 1) == 0.0f, "the second column needs no scroll");
}

void paged() {
    const HomeLayout layout{window(37, ScrollMode::Paged)};
    expect(layout.columns() == 5, "WiiSu sizes this window to 5 columns");
    expect(layout.pageCount() == 3, "37 items in 15-slot pages make 3 pages");
    expect(layout.slotCount() == 45, "every page is whole");
    expect(layout.pageOf(14) == 0 && layout.pageOf(15) == 1 && layout.pageOf(36) == 2,
           "items fill page by page");
    expect(layout.cellOf(15).left == 0 && layout.cellOf(15).top == 0,
           "a page starts at its own first column");
    near(layout.viewportX(), 0.0f, "Paged has no flow inset");
    const float pageGap = 36.0f * 1.5f;
    const float pageWidth = 5.0f * layout.cellWidth() + 4.0f * layout.gap();
    near(layout.pageStride(), pageWidth + pageGap, "page stride is a page plus the 36 dp gap");
    near(layout.pageSidePadding(), (1280.0f - pageWidth) * 0.5f, "a page that fits is centred");
    // The next page's first column peeks in from the right edge.
    const float peekX = layout.canvasRect(15, layout.pageScroll(0)).x;
    near(peekX, 1280.0f - (layout.pageSidePadding() - pageGap), "next page peeks past the gap");
    near(layout.canvasRect(15, layout.pageScroll(1)).x, layout.pageSidePadding(),
         "page 1 sits where page 0 did");
}

void pagedShrink() {
    // Tall enough that height alone would make cells too wide for the page.
    const HomeLayout layout{HomeLayoutInput{
        .width = 1000.0f, .height = 1200.0f, .dp = 1.0f, .items = 20, .mode = ScrollMode::Paged}};
    expect(layout.columns() == 3, "a WiiSu page has at least 3 columns");
    const float pageWidth = 3.0f * layout.cellWidth() + 2.0f * layout.gap();
    const float peek = layout.pageSidePadding() - 36.0f;
    expect(peek > 0.0f && peek <= 6.0f + 1e-3f, "shrunk cells leave at most the 6 dp peek");
    near(pageWidth + 2.0f * (peek + 36.0f), 1000.0f,
         "a page, both gaps and both peeks fill the width");
}

void columnGrowth() {
    expect(HomeLayout::wiiSuPageColumns(1280.0f, 664.0f, 3, 23.0f, 54.0f, 9.0f) == 5,
           "1280 px with 206 px cells fits 5 columns");
    expect(HomeLayout::wiiSuPageColumns(3000.0f, 664.0f, 3, 23.0f, 54.0f, 9.0f) == 13,
           "growth is unlimited without bottom navigation");
    expect(HomeLayout::wiiSuPageColumns(400.0f, 800.0f, 3, 10.0f, 36.0f, 6.0f) == 3,
           "never fewer than 3 columns");
}

void pill() {
    expect(HomeLayout{window(37, ScrollMode::Flow)}.pagePill(0).dots.empty(), "no pill in Flow");
    expect(HomeLayout{window(15, ScrollMode::Paged)}.pagePill(0).dots.empty(),
           "no pill for one page");
    const iideck::ui::PagePill pill = HomeLayout{window(37, ScrollMode::Paged)}.pagePill(1);
    // 800 / 1.5 = 533 dp short side, so not compact: 25 dp tall, 9 dp padding, 10 dp slots.
    near(pill.body.height, 37.5f, "pill is 25 dp tall");
    near(pill.body.width, 88.5f, "pill hugs three dots");
    near(pill.body.x, (1280.0f - 88.5f) * 0.5f, "pill is centred");
    near(pill.body.y, 51.0f, "pill sits 34 dp from the top");
    expect(pill.dots.size() == 3, "one dot per page");
    near(pill.dots[0].x, 616.75f, "first dot centre");
    near(pill.dots[1].x - pill.dots[0].x, 23.25f, "dots are a 10 dp slot plus 5.5 dp apart");
    expect(!pill.dots[0].active && pill.dots[1].active, "the current page's dot is active");
    near(pill.dots[1].radius, 3.1f * 1.5f, "active dot radius 3.1 dp");
    near(pill.dots[0].radius, 2.7f * 1.5f, "inactive dot radius 2.7 dp");
    near(pill.haloRadius, 5.0f * 1.5f, "halo radius 5 dp");

    // 480 dp short side: compactness 0.25, scale 0.96.
    const iideck::ui::PagePill compact = HomeLayout{
        HomeLayoutInput{
            .width = 853.0f,
            .height = 480.0f,
            .dp = 1.0f,
            .items = 60,
            .mode = ScrollMode::Paged}}.pagePill(0);
    near(compact.body.height, 24.0f, "compact pill shrinks by the scale");
    near(compact.body.y, 31.25f, "compact pill moves up by 11 dp x compactness");
}

} // namespace

int main() {
    defaultViewport();
    gapRule();
    flowMetrics();
    columnMajor();
    flowScrollKeepsFocusVisible();
    paged();
    pagedShrink();
    columnGrowth();
    pill();
    std::printf("home_layout: all checks passed\n");
    return 0;
}
