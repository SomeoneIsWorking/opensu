// HomeLayout against iiSU's grid numbers (docs/reference/iisu/home-grid.md §1).
#include "home_layout.hpp"

#include <cstdio>

#include "check.hpp"

namespace {

using opensu::test::expect;
using opensu::test::fail;
using opensu::test::near;
using opensu::ui::HomeLayout;
using opensu::ui::HomeLayoutInput;
using opensu::ui::ScrollMode;

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
    expect(layout.slotCount() == 48, "empty slots fill four 3 x 4 viewports");
    expect(layout.columns() == 16, "48 slots in 3 rows make 16 columns");
    const HomeLayout full{window(49, ScrollMode::Flow)};
    expect(full.slotCount() == 60, "49 items take a fifth viewport, filled with empty slots");
    const HomeLayout empty{window(0, ScrollMode::Flow)};
    expect(empty.slotCount() == 48, "an empty grid still shows four viewports of slots");
    near(layout.cellWidth(), layout.cellHeight(), "cells are square");
    expect(HomeLayout::clampRows(0) == 1, "rows are at least 1");
    expect(HomeLayout::clampRows(9) == 6, "rows are at most 6");
    expect(HomeLayout::clampColumns(0) == 1, "horizontal columns are at least 1");
}

void categoryLevel() {
    HomeLayoutInput input = window(5, ScrollMode::Flow);
    input.fillSlots = false;
    const HomeLayout layout{input};
    expect(layout.slotCount() == 5, "the category level holds its five consoles and no slots");
    expect(layout.columns() == 2, "five items in 3 rows make 2 columns");
    input.mode = ScrollMode::Paged;
    const HomeLayout paged{input};
    expect(paged.slotCount() == 5 && paged.pageCount() == 1, "one page of items has no pill");
    input.items = 0;
    expect(HomeLayout{input}.slotCount() == 1, "an empty grid keeps one cell for focus");
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
    near(layout.paddingLeft(), 0.0f, "flow starts at the viewport inset");
    near(layout.maxScroll(), 2972.64704f, "flow scrolls until the last column is centred", 1e-2);
}

void columnMajor() {
    const HomeLayout layout{window(12, ScrollMode::Flow)};
    const auto cell = [&layout](std::size_t index, int column, int row) {
        const opensu::ui::GridCell got = layout.cellOf(index);
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
        const opensu::ui::Rect rect = layout.contentRect(index);
        return rect.x >= target - 0.01f && rect.right() <= target + viewport + 0.01f;
    };
    for (std::size_t column = 1; column < 13; ++column) {
        target = layout.scrollTarget({column * 3, target, 1});
        expect(visible(column * 3), "moving right keeps the focused column in view");
        expect(target >= 0.0f && target <= layout.maxScroll(), "target stays in range");
    }
    for (std::size_t column = 12; column-- > 0;) {
        target = layout.scrollTarget({column * 3, target, -1});
        expect(visible(column * 3), "moving left keeps the focused column in view");
    }
    near(target, 0.0f, "the first column scrolls back to the start");
    // A step right from the start leads by clamp(1.85 pitch, 0.12 vp, 0.32 vp) before moving.
    expect(layout.scrollTarget({3, 0.0f, 1}) == 0.0f, "the second column needs no scroll");
}

void paged() {
    const HomeLayout layout{window(37, ScrollMode::Paged)};
    expect(layout.columns() == 5, "WiiSu sizes this window to 5 columns");
    expect(layout.pageCount() == 4, "37 items in 15-slot pages show at least 4 pages");
    expect(layout.slotCount() == 60, "every page is whole");
    expect(HomeLayout{window(61, ScrollMode::Paged)}.pageCount() == 5,
           "61 items need a fifth page");
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
    expect(HomeLayout::wiiSuPageColumns({1280.0f, 664.0f, 3, 23.0f, 54.0f, 9.0f}) == 5,
           "1280 px with 206 px cells fits 5 columns");
    expect(HomeLayout::wiiSuPageColumns({3000.0f, 664.0f, 3, 23.0f, 54.0f, 9.0f}) == 13,
           "growth is unlimited without bottom navigation");
    expect(HomeLayout::wiiSuPageColumns({400.0f, 800.0f, 3, 10.0f, 36.0f, 6.0f}) == 3,
           "never fewer than 3 columns");
}

void pill() {
    expect(HomeLayout{window(37, ScrollMode::Flow)}.pagePill(0).dots.empty(), "no pill in Flow");
    const opensu::ui::PagePill pill = HomeLayout{window(37, ScrollMode::Paged)}.pagePill(1);
    // 800 / 1.5 = 533 dp short side, so not compact: 25 dp tall, 9 dp padding, 10 dp slots.
    near(pill.body.height, 37.5f, "pill is 25 dp tall");
    near(pill.body.width, 111.75f, "pill hugs four dots");
    near(pill.body.x, (1280.0f - 111.75f) * 0.5f, "pill is centred");
    near(pill.body.y, 51.0f, "pill sits 34 dp from the top");
    expect(pill.dots.size() == 4, "one dot per page");
    near(pill.dots[0].x, 605.125f, "first dot centre");
    near(pill.dots[1].x - pill.dots[0].x, 23.25f, "dots are a 10 dp slot plus 5.5 dp apart");
    expect(!pill.dots[0].active && pill.dots[1].active, "the current page's dot is active");
    near(pill.dots[1].radius, 3.1f * 1.5f, "active dot radius 3.1 dp");
    near(pill.dots[0].radius, 2.7f * 1.5f, "inactive dot radius 2.7 dp");
    near(pill.haloRadius, 5.0f * 1.5f, "halo radius 5 dp");

    // 480 dp short side: compactness 0.25, scale 0.96.
    const opensu::ui::PagePill compact = HomeLayout{
        HomeLayoutInput{
            .width = 853.0f,
            .height = 480.0f,
            .dp = 1.0f,
            .items = 60,
            .mode = ScrollMode::Paged}}.pagePill(0);
    near(compact.body.height, 24.0f, "compact pill shrinks by the scale");
    near(compact.body.y, 31.25f, "compact pill moves up by 11 dp x compactness");
}

void referenceCapture() {
    // iiSU in the emulator at 1920 x 1080, 2.25 px/dp, 853 x 456 dp after the status bar; grid
    // insets are TopBarMetrics' 69.29 and 68.94 dp. Edges measured from the captures, +-1.5 px.
    const auto at = [](ScrollMode mode) {
        return HomeLayout{HomeLayoutInput{.width = 1920.0f,
                                          .height = 1080.0f,
                                          .dp = 2.25f,
                                          .items = 0,
                                          .mode = mode,
                                          .topInset = 69.29424f * 2.25f,
                                          .bottomInset = 68.93672f * 2.25f}};
    };
    const HomeLayout standard = at(ScrollMode::Flow);
    near(standard.canvasRect(0, 0.0f).x, 18.0f, "Standard: first column at x 18", 1.5);
    near(standard.canvasRect(0, 0.0f).y, 173.5f, "Standard: first row at y 173.5", 1.5);
    near(standard.cellWidth(), 226.0f, "Standard: 226 px cells", 1.5);
    near(standard.cellWidth() + standard.gap(), 253.7f, "Standard: 253.7 px pitch", 1.5);
    const HomeLayout wiisu = at(ScrollMode::Paged);
    expect(wiisu.columns() == 7, "WiiSu: 7 columns a page");
    expect(wiisu.pageCount() == 4, "WiiSu: 4 pages");
    near(wiisu.canvasRect(0, 0.0f).x, 94.5f, "WiiSu: page starts at x 94.5", 1.5);
    near(wiisu.canvasRect(0, 0.0f).y, 173.5f, "WiiSu: first row at y 173.5", 1.5);
    near(wiisu.cellWidth(), 223.5f, "WiiSu: 223.5 px cells", 1.5);
    near(wiisu.cellWidth() + wiisu.gap(), 251.0f, "WiiSu: 251 px pitch", 1.5);
}

void arrows() {
    expect(!HomeLayout{window(37, ScrollMode::Flow)}.pageArrows(0).next, "no arrows in Flow");
    // 1920 x 1080 at 2.25 px/dp, as iiSU's reference capture: 64 x 96 px, 26 px from the edge.
    const HomeLayout layout{HomeLayoutInput{
        .width = 1920.0f, .height = 1080.0f, .dp = 2.25f, .items = 0, .mode = ScrollMode::Paged}};
    const opensu::ui::PageArrows first = layout.pageArrows(0);
    expect(!first.previous, "the first page only points on");
    if (!first.next) {
        fail("the first page only points on");
    }
    near(first.next->x, 1920.0f - 26.0f - 64.0f, "next arrow sits 26 px from the right edge");
    near(first.next->y, 492.0f, "arrows are vertically centred");
    near(first.next->width, 64.0f, "arrow width is capped at 64 px");
    near(first.next->height, 96.0f, "arrow height is 1.52 x width, capped at 96 px");
    const opensu::ui::PageArrows middle = layout.pageArrows(1);
    if (!middle.previous || !middle.next) {
        fail("a middle page points both ways");
    }
    near(middle.previous->x, 26.0f, "previous arrow sits 26 px from the left edge");
    const opensu::ui::PageArrows last = layout.pageArrows(layout.pageCount() - 1);
    expect(last.previous && !last.next, "the last page only points back");
}

void slotsUnderThePointer() {
    const HomeLayout layout{window(37, ScrollMode::Flow)};
    const float scroll = 0.0f;
    const opensu::ui::Rect first = layout.canvasRect(0, scroll);
    const auto hit = layout.slotAt(scroll, first.centreX(), first.centreY());
    expect(hit && *hit == 0, "a cell's centre hits it");
    const opensu::ui::Rect third = layout.canvasRect(2, scroll);
    const auto lower = layout.slotAt(scroll, third.x + 1.0f, third.bottom() - 1.0f);
    expect(lower && *lower == 2, "so does its corner");
    expect(!layout.slotAt(scroll, first.right() + layout.gap() * 0.5f, first.centreY()),
           "the gap between cells hits none");
    expect(!layout.slotAt(scroll, first.x - 1.0f, first.centreY()), "nor does the margin");
    const opensu::ui::Rect placeholder = layout.canvasRect(40, scroll);
    const auto empty = layout.slotAt(scroll, placeholder.centreX(), placeholder.centreY());
    expect(empty && *empty == 40, "an empty slot is hit like a tile");
    const opensu::ui::Rect scrolled = layout.canvasRect(0, 100.0f);
    const auto moved = layout.slotAt(100.0f, scrolled.centreX(), scrolled.centreY());
    expect(moved && *moved == 0, "the scroll offset moves the cells");
}

void pagesUnderThePointer() {
    const HomeLayout layout{window(37, ScrollMode::Paged)};
    const opensu::ui::PageArrows arrows = layout.pageArrows(1);
    if (!arrows.previous || !arrows.next) {
        fail("a middle page has both arrows");
    }
    const auto previous = layout.pageAt(1, arrows.previous->centreX(), arrows.previous->centreY());
    const auto next = layout.pageAt(1, arrows.next->centreX(), arrows.next->centreY());
    expect(previous && *previous == 0, "the left arrow goes back a page");
    expect(next && *next == 2, "the right arrow goes on");
    expect(!layout.pageAt(0, arrows.previous->centreX(), arrows.previous->centreY()),
           "the first page has no left arrow");
    const opensu::ui::PagePill pill = layout.pagePill(1);
    for (std::size_t i = 0; i < pill.dots.size(); ++i) {
        const auto dot = layout.pageAt(1, pill.dots[i].x, pill.dots[i].y);
        expect(dot && *dot == static_cast<int>(i), "a dot goes to its page");
    }
    expect(!layout.pageAt(1, pill.body.x - 5.0f, pill.body.centreY()), "outside the pill is none");
    expect(!HomeLayout{window(37, ScrollMode::Flow)}.pageAt(0, 640.0f, 40.0f),
           "Flow has no page controls");
}

void slotOnPage() {
    const HomeLayout layout{window(37, ScrollMode::Paged)};
    expect(layout.slotOnPage(1, 1, false) == 15 + 1, "the first column's slot in the row");
    expect(layout.slotOnPage(1, 1, true) == 15 + 4 * 3 + 1, "the last column's slot in the row");
    expect(layout.slotOnPage(2, 0, false) == 30, "page 2 starts at slot 30");
}

} // namespace

int main() {
    defaultViewport();
    categoryLevel();
    gapRule();
    flowMetrics();
    columnMajor();
    flowScrollKeepsFocusVisible();
    paged();
    pagedShrink();
    columnGrowth();
    pill();
    arrows();
    referenceCapture();
    slotsUnderThePointer();
    pagesUnderThePointer();
    slotOnPage();
    std::printf("home_layout: all checks passed\n");
    return 0;
}
