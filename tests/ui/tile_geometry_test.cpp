// Tile geometry against iiSU tj2.V and the border sprite (docs/reference/iisu/home-grid.md §3).
#include "tile_geometry.hpp"

#include <cstdio>

#include "check.hpp"

namespace {

using iideck::test::near;
using iideck::ui::Rect;

void cell200() {
    // home-grid.md §3.2's worked example.
    const iideck::ui::TileGeometry g = iideck::ui::tileGeometry(Rect{0, 0, 200, 200}, 200.0f);
    near(g.frameWidth, 9.0f, "frame is 0.05 x side, at most 9");
    near(g.contentRadius, 14.0f, "content radius is ceil(0.068 x cell)");
    near(g.outerRadius, 23.0f, "outer radius is frame plus content radius");
    near(g.outerStroke, 2.25f, "outer stroke is 0.25 x frame, at most 2.25");
    near(g.content.x, 9.0f, "content is inset by the frame");
    near(g.content.width, 182.0f, "content is inset on both sides");
}

void smallTile() {
    const iideck::ui::TileGeometry g = iideck::ui::tileGeometry(Rect{0, 0, 20, 20}, 20.0f);
    near(g.frameWidth, 1.5f, "frame is at least 1.5");
    near(g.contentRadius, 4.0f, "content radius is at least 4");
    near(g.outerStroke, 1.0f, "outer stroke is at least 1");
}

void frame() {
    const iideck::ui::FrameGeometry f = iideck::ui::frameGeometry(Rect{0, 0, 512, 512});
    near(f.stroke, 13.0f, "stroke is 26/1024 of the side");
    near(f.tab, 90.0f, "tab is 180/1024 of the side");
    near(f.outerRadius, 40.0f, "corner is 80/1024 of the side");
    near(f.artRadius, 46.08f, "art is clipped at 9% of the side");
}

void glyph() {
    // home-grid.md §3.7 step 4: 45/1024 in, 90/1024 square, which is the tab's centre.
    const iideck::ui::FrameGeometry f = iideck::ui::frameGeometry(Rect{100, 200, 512, 512});
    near(f.glyph.x, 100.0f + 22.5f, "glyph square starts 45/1024 in");
    near(f.glyph.y, 200.0f + 22.5f, "glyph square starts 45/1024 down");
    near(f.glyph.width, 45.0f, "glyph square is 90/1024 of the side");
    near(f.glyph.height, 45.0f, "glyph square is square");
    near(f.glyph.centreX(), 100.0f + f.tab * 0.5f,
         "glyph square is centred in the tab horizontally");
    near(f.glyph.centreY(), 200.0f + f.tab * 0.5f, "glyph square is centred in the tab vertically");
}

void contain() {
    const Rect slot{10, 20, 40, 40};
    const Rect tall = iideck::ui::containFit(38.0f, 95.0f, slot);
    near(tall.height, 40.0f, "a tall glyph fills the slot's height");
    near(tall.width, 16.0f, "a tall glyph keeps its aspect");
    near(tall.centreX(), slot.centreX(), "a tall glyph is centred across");
    near(tall.y, 20.0f, "a tall glyph starts at the slot's top");
    const Rect wide = iideck::ui::containFit(80.0f, 20.0f, slot);
    near(wide.width, 40.0f, "a wide glyph fills the slot's width");
    near(wide.height, 10.0f, "a wide glyph keeps its aspect");
    near(wide.centreY(), slot.centreY(), "a wide glyph is centred down");
    near(iideck::ui::containFit(0.0f, 5.0f, slot).width, 0.0f, "an empty image fits nothing");
}

void storeIcons() {
    const Rect content{100, 200, 200, 300};
    const iideck::ui::StoreIconRow none = iideck::ui::storeIconRow(content, 0);
    iideck::test::expect(none.count == 0, "no stores, no badges");

    const iideck::ui::StoreIconRow one = iideck::ui::storeIconRow(content, 1);
    iideck::test::expect(one.count == 1, "one store, one badge");
    near(one.badges[0].width, 30.0f, "a badge is 15% of the short side");
    near(one.badges[0].height, 30.0f, "a badge is round");
    near(one.badges[0].right(), content.right() - 10.0f, "a badge is 5% in from the right");
    near(one.badges[0].bottom(), content.bottom() - 10.0f, "a badge is 5% up from the bottom");

    const iideck::ui::StoreIconRow two = iideck::ui::storeIconRow(content, 2);
    near(two.badges[1].x, one.badges[0].x, "the last badge sits against the corner");
    near(two.badges[1].x - two.badges[0].x, 36.0f, "badges are a fifth of a badge apart");
    near(two.badges[0].y, two.badges[1].y, "badges share a line");

    const iideck::ui::StoreIconRow many = iideck::ui::storeIconRow(content, 9);
    iideck::test::expect(many.count == iideck::ui::maxStoreIcons, "a row holds one badge per store");
    iideck::test::expect(many.badges[0].x > content.x, "the row stays on the tile");
}

} // namespace

int main() {
    cell200();
    smallTile();
    frame();
    glyph();
    contain();
    storeIcons();
    std::printf("tile_geometry: all checks passed\n");
    return 0;
}
