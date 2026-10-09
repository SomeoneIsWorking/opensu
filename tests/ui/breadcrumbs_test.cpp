// The breadcrumb trail's geometry: cells in order, fitting to the room, and what a point hits.
#include "breadcrumbs.hpp"

#include <cstdio>

#include "check.hpp"

namespace {

using namespace opensu::ui;
using opensu::test::expect;
using opensu::test::need;

BreadcrumbFrame frame(float maxRight) {
    return BreadcrumbFrame{
        .left = 20.0f, .top = 10.0f, .height = 40.0f, .maxRight = maxRight, .dp = 1.0f};
}

void cellsRunInOrder() {
    const BreadcrumbLayout layout = layoutBreadcrumbs(frame(1000.0f), {60.0f, 40.0f, 90.0f});
    expect(layout.cells.size() == 3 && layout.chevrons.size() == 2,
           "a cell per level, a chevron between");
    expect(layout.cells[0].x > layout.bar.x && layout.cells[0].right() < layout.cells[1].x,
           "cells run left to right inside the bar");
    expect(layout.cells[1].right() < layout.chevrons[1] + 1.0f &&
               layout.chevrons[0] > layout.cells[0].right() &&
               layout.chevrons[0] < layout.cells[1].x,
           "a chevron stands between two cells");
    expect(layout.bar.right() > layout.cells[2].right(), "the bar encloses the last cell");
    expect(layout.textWidths[2] == 90.0f, "a trail that fits keeps its widths");
}

void longTrailFits() {
    const BreadcrumbLayout layout = layoutBreadcrumbs(frame(300.0f), {50.0f, 200.0f, 200.0f});
    expect(layout.bar.right() <= 300.0f + 0.5f, "the bar stays left of the limit");
    expect(layout.textWidths[0] == 50.0f, "the short level stays whole");
    expect(layout.textWidths[1] == layout.textWidths[2] && layout.textWidths[1] < 200.0f,
           "the long levels give up width evenly");
    const BreadcrumbLayout tight = layoutBreadcrumbs(frame(60.0f), {200.0f, 200.0f});
    expect(tight.textWidths[0] >= crumbMinTextDp, "no level shrinks past its floor");
}

void hitTesting() {
    const BreadcrumbLayout layout = layoutBreadcrumbs(frame(1000.0f), {60.0f, 40.0f});
    for (std::size_t i = 0; i < layout.cells.size(); ++i) {
        expect(need(layout.crumbAt(layout.cells[i].centreX(), layout.cells[i].centreY()), "cell") ==
                   i,
               "a cell's centre hits it");
    }
    expect(!layout.crumbAt(layout.chevrons[0], layout.cells[0].centreY()),
           "a chevron hits no level");
    expect(!layout.crumbAt(layout.cells[0].centreX(), layout.cells[0].bottom() + 5.0f),
           "below the bar hits none");
}

void places() {
    expect(isPlace(Crumb{"Library", CrumbKind::Section, opensu::library::Section::Library}) &&
               isPlace(Crumb{"GOG", CrumbKind::Folder}) && isPlace(Crumb{"had", CrumbKind::Search}),
           "sections, folders and a search can be returned to");
    expect(!isPlace(Crumb{"Installed", CrumbKind::Filter}) &&
               !isPlace(Crumb{"Hades", CrumbKind::Game}),
           "a filter and the open game cannot");
}

} // namespace

int main() {
    cellsRunInOrder();
    longTrailFits();
    hitTesting();
    places();
    std::puts("breadcrumbs: ok");
    return 0;
}
