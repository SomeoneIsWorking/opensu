// The XMB and the Carousel against navigation.md §2.2's worked numbers.
#include "rail_layout.hpp"

#include <cmath>
#include <cstdio>

#include "check.hpp"

namespace {

using opensu::test::expect;
using opensu::test::near;
using opensu::ui::CarouselLayout;
using opensu::ui::RailInput;
using opensu::ui::Rect;
using opensu::ui::XmbLayout;

RailInput input(float width, float height, std::size_t items, float focus) {
    return RailInput{width, height, std::vector<float>(items, 1.0f), focus, 9};
}

void iconScales() {
    near(opensu::ui::iconScale(9), 1.34, "level 9 is 1.34");
    near(opensu::ui::iconScale(10), 1.45, "level 10 is 1.45");
    near(opensu::ui::iconScale(1), 0.67, "level 1 floors at 0.67");
    near(opensu::ui::iconScale(20), 2.55, "level 20 caps at 2.55");
    near(opensu::ui::iconScale(0), 0.67, "a level under 1 reads as 1");
    near(opensu::ui::iconScale(99), 2.55, "a level over 20 reads as 20");
}

void xmbWorkedNumbers() {
    // 1920 x 1080, level 9: focused 434, unfocused 217, gap 17, left edge 340.5, centre y 540.
    const XmbLayout xmb{input(1920.0f, 1080.0f, 5, 2.0f)};
    near(xmb.focusedSize(), 434.2, "focused size is 0.30 H k", 0.1);
    near(xmb.unfocusedSize(), 217.1, "the others 0.15 H k", 0.1);
    near(xmb.gap(), 17.4, "the gap is 8% of the small size", 0.1);
    near(xmb.leftMargin(), 340.5, "the left edge is the measured 340.5 px, not 0.185 W", 0.01);
    near(xmb.centreY(), 540.0, "centred vertically", 0.001);

    const Rect& focused = xmb.rects()[2];
    near(focused.x, 340.5, "the focused tile's left edge", 0.1);
    near(focused.centreY(), 540.0, "is centred vertically", 0.01);
    near(focused.width, 434.2, "square", 0.1);
    near(focused.height, 434.2, "square", 0.1);
    for (std::size_t i : {0U, 1U, 3U, 4U}) {
        near(xmb.rects()[i].x, 340.5, "every tile shares the left edge", 0.01);
        near(xmb.rects()[i].width, 217.1, "the others are the small size", 0.1);
    }
    near(xmb.rects()[3].y, focused.bottom() + 17.4, "the next tile is a gap below", 0.1);
    near(xmb.rects()[1].bottom(), focused.y - 17.4, "the previous one a gap above", 0.1);
    near(xmb.rects()[4].y, xmb.rects()[3].bottom() + 17.4, "and so on down", 0.1);
}

void xmbAnchors() {
    // navigation.md §5.3 on 1920 x 1080 at 2.25 px per dp.
    RailInput in = input(1920.0f, 1080.0f, 5, 0.0f);
    in.dp = 2.25f;
    const XmbLayout xmb{in};
    near(xmb.sectionIcon().width, 166.5, "the section icon is 74 dp wide", 0.1);
    near(xmb.sectionIcon().height, 99.0, "and 44 dp tall", 0.1);
    near(xmb.sectionIcon().centreX(), 187.0, "centred at x 187", 0.01);
    near(xmb.sectionIcon().centreY(), 540.0, "on the centre line", 0.01);
    near(xmb.headerCard().width, 162.0, "the console card is 72 dp", 0.1);
    near(xmb.headerCard().centreX(), 188.5, "centred at x 188.5", 0.01);
    near(xmb.markerX(), 313.5, "the marker is at x 313.5", 0.01);
    near(xmb.titleLeft(), 340.5 + 434.2 + 38.5, "the title starts 17.1 dp past the focused slot",
         0.2);
    near(xmb.titleCapTop(), 360.0, "its capitals' tops stand at y 360", 0.6);
    near(xmb.titleCapHeight(), 58.0, "and are 58 px tall", 0.1);
}

void xmbBetweenTiles() {
    // Half way between tiles 1 and 2 they are the same size and sit either side of the centre.
    const XmbLayout xmb{input(1920.0f, 1080.0f, 4, 1.5f)};
    near(xmb.rects()[1].height, xmb.rects()[2].height, "equal sizes half way", 0.01);
    near(xmb.sizeOf(1), 217.1 + (434.2 - 217.1) * 0.5, "a smoothstep of 0.5 is 0.5", 0.1);
    near(xmb.rects()[1].centreY() + xmb.rects()[2].centreY(), 1080.0, "balanced about the centre",
         0.01);
    near(xmb.rects()[2].y - xmb.rects()[1].bottom(), xmb.gap(), "a gap apart", 0.01);

    // The layout moves continuously: a tile's y at 1.00 and 1.01 differ by a sliver.
    const XmbLayout a{input(1920.0f, 1080.0f, 4, 1.0f)};
    const XmbLayout b{input(1920.0f, 1080.0f, 4, 1.01f)};
    for (std::size_t i = 0; i < 4; ++i) {
        expect(std::abs(a.rects()[i].y - b.rects()[i].y) < 10.0f, "no jump for a small move");
    }
}

void xmbEdges() {
    const XmbLayout past{input(1920.0f, 1080.0f, 3, 9.0f)};
    const XmbLayout last{input(1920.0f, 1080.0f, 3, 2.0f)};
    near(past.rects()[2].y, last.rects()[2].y, "a focus past the end is the last tile", 0.001);
    const XmbLayout before{input(1920.0f, 1080.0f, 3, -4.0f)};
    near(before.rects()[0].centreY(), 540.0, "a focus before the start is the first", 0.001);
    const XmbLayout none{input(1920.0f, 1080.0f, 0, 0.0f)};
    expect(none.rects().empty(), "no tiles, no rectangles");
}

void xmbAspects() {
    // A wide tile shrinks to the column's width; a tall one keeps the height and is narrower.
    RailInput in = input(1920.0f, 1080.0f, 3, 1.0f);
    in.aspects = {2.0f, 1.0f, 0.5f};
    const XmbLayout xmb{in};
    near(xmb.rects()[0].width, 217.1, "wide: the column's width", 0.1);
    near(xmb.rects()[0].height, 217.1 / 2.0, "at its own aspect", 0.1);
    near(xmb.rects()[2].height, 217.1, "tall: the column's height", 0.1);
    near(xmb.rects()[2].width, 217.1 / 2.0, "as narrow as its art", 0.1);
    near(xmb.rects()[1].y - xmb.rects()[0].bottom(), xmb.gap(), "gaps follow the real heights",
         0.01);
}

void carouselWorkedNumbers() {
    // 1920 x 1080, level 9: focused 299, unfocused 200, gap 30, centre x 960, baseline 918.
    const CarouselLayout row{input(1920.0f, 1080.0f, 5, 2.0f)};
    near(row.focusedSize(), 299.4, "focused size is 0.30 H k'", 0.1);
    near(row.unfocusedSize(), 199.6, "the others 0.20 H k'", 0.1);
    near(row.gap(), 29.9, "the gap is 15% of the small size", 0.1);
    near(row.centreX(), 960.0, "centred on half the width", 0.001);
    near(row.baselineY(), 918.0, "standing on 85% of the height", 0.001);

    const Rect& focused = row.rects()[2];
    near(focused.centreX(), 960.0, "the focused tile is centred", 0.01);
    near(focused.bottom(), 918.0, "and stands on the baseline", 0.01);
    near(focused.width, 299.4, "square", 0.1);
    for (std::size_t i : {0U, 1U, 3U, 4U}) {
        near(row.rects()[i].bottom(), 918.0, "every tile stands on it", 0.01);
        near(row.rects()[i].height, 199.6, "the others are the small size", 0.1);
    }
    near(row.rects()[3].x, focused.right() + 29.9, "the next tile is a gap to the right", 0.1);
    near(row.rects()[1].right(), focused.x - 29.9, "the previous one a gap to the left", 0.1);
}

void carouselAnchors() {
    RailInput in = input(1920.0f, 1080.0f, 5, 0.0f);
    in.dp = 2.25f;
    const CarouselLayout row{in};
    near(row.titleCapTop(), 364.0, "the title's capitals start at y 364", 0.01);
    near(row.titleCapHeight(), 29.0, "and are 29 px tall", 0.1);
    near(row.markerY(), 946.0, "the marker under a game is at y 946", 0.01);
    near(row.rects()[0].centreX(), 960.0, "the first tile is the centred one", 0.01);
}

void carouselSlots() {
    RailInput in = input(1920.0f, 1080.0f, 3, 1.0f);
    in.aspects = {0.3f, 1.5f, 4.0f};
    // Tile 1 is focused; tile 0 is narrow, tile 2 wide, both clamped.
    const CarouselLayout row{in};
    near(row.rects()[0].width, 199.6 * 0.55, "a narrow tile's slot floors at 0.55", 0.1);
    near(row.rects()[1].width, 299.4 * 1.5, "a wide tile's slot follows its art", 0.1);
    near(row.rects()[2].width, 199.6 * 2.5, "a wider one caps at 2.5", 0.1);
    near(row.rects()[2].x - row.rects()[1].right(), row.gap(), "gaps follow the real widths", 0.01);
    near(row.rects()[2].height, 199.6, "heights do not change", 0.1);
}

void carouselBetween() {
    const CarouselLayout row{input(1920.0f, 1080.0f, 4, 1.5f)};
    near(row.rects()[1].width, row.rects()[2].width, "equal sizes half way", 0.01);
    near(row.rects()[1].centreX() + row.rects()[2].centreX(), 1920.0, "balanced about the centre",
         0.01);
    const CarouselLayout small{input(853.0f, 456.0f, 3, 0.0f)};
    near(small.baselineY(), 0.85 * 456.0, "the baseline scales with the canvas", 0.01);
    near(small.rects()[0].centreX(), 426.5, "the first tile starts at the centre", 0.01);
}

void tileUnderThePointer() {
    const XmbLayout xmb{input(1920.0f, 1080.0f, 5, 2.0f)};
    const Rect& focused = xmb.rects()[2];
    const auto hit =
        opensu::ui::railTileAt(xmb.rects(), 2.0f, focused.centreX(), focused.centreY());
    expect(hit && *hit == 2, "the focused tile is hit at its centre");
    const Rect& above = xmb.rects()[1];
    const auto upper = opensu::ui::railTileAt(xmb.rects(), 2.0f, above.centreX(), above.centreY());
    expect(upper && *upper == 1, "so is a neighbour");
    expect(!opensu::ui::railTileAt(xmb.rects(), 2.0f, 5.0f, 5.0f), "the corner hits none");
    // Where two rectangles overlap, the one nearer the focus is drawn last and wins.
    const std::vector<Rect> stacked{Rect{0.0f, 0.0f, 100.0f, 100.0f},
                                    Rect{50.0f, 0.0f, 100.0f, 100.0f}};
    const auto lowFocus = opensu::ui::railTileAt(stacked, 0.0f, 75.0f, 50.0f);
    const auto highFocus = opensu::ui::railTileAt(stacked, 1.0f, 75.0f, 50.0f);
    expect(lowFocus && *lowFocus == 0 && highFocus && *highFocus == 1,
           "the tile nearer the focus is on top");
    const CarouselLayout row{input(1920.0f, 1080.0f, 4, 1.0f)};
    const auto across = opensu::ui::railTileAt(row.rects(), 1.0f, row.rects()[3].centreX(),
                                               row.rects()[3].centreY());
    expect(across && *across == 3, "the carousel hits by the same rule");
}

} // namespace

int main() {
    tileUnderThePointer();
    iconScales();
    xmbWorkedNumbers();
    xmbAnchors();
    xmbBetweenTiles();
    xmbEdges();
    xmbAspects();
    carouselWorkedNumbers();
    carouselAnchors();
    carouselSlots();
    carouselBetween();
    std::printf("rail_layout: all checks passed\n");
    return 0;
}
