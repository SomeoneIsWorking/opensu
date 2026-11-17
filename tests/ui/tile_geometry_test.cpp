// Tile geometry against iiSU tj2.V and the border sprite (docs/reference/iisu/home-grid.md §3).
#include "tile_geometry.hpp"

#include <cstdio>

#include "check.hpp"
#include "support/test_main.hpp"

namespace {

using opensu::test::expect;
using opensu::test::near;
using opensu::ui::Rect;

void outerForContent() {
    for (const float side : {434.0f, 217.0f, 120.0f, 40.0f}) {
        const Rect content{100.0f, 50.0f, side, side};
        const Rect outer = opensu::ui::outerForContent(content);
        const opensu::ui::TileGeometry geometry = opensu::ui::tileGeometry(outer, side);
        near(geometry.content.x, content.x, "the content's left edge is the one asked for", 0.01);
        near(geometry.content.width, content.width, "and its width", 0.01);
        expect(outer.width > content.width, "the outer rectangle is the larger");
    }
}

void cell200() {
    // home-grid.md §3.2's worked example.
    const opensu::ui::TileGeometry g = opensu::ui::tileGeometry(Rect{0, 0, 200, 200}, 200.0f);
    near(g.frameWidth, 9.0f, "frame is 0.05 x side, at most 9");
    near(g.contentRadius, 14.0f, "content radius is ceil(0.068 x cell)");
    near(g.outerRadius, 23.0f, "outer radius is frame plus content radius");
    near(g.outerStroke, 2.25f, "outer stroke is 0.25 x frame, at most 2.25");
    near(g.content.x, 9.0f, "content is inset by the frame");
    near(g.content.width, 182.0f, "content is inset on both sides");
}

void smallTile() {
    const opensu::ui::TileGeometry g = opensu::ui::tileGeometry(Rect{0, 0, 20, 20}, 20.0f);
    near(g.frameWidth, 1.5f, "frame is at least 1.5");
    near(g.contentRadius, 4.0f, "content radius is at least 4");
    near(g.outerStroke, 1.0f, "outer stroke is at least 1");
}

void frame() {
    const opensu::ui::FrameGeometry f = opensu::ui::frameGeometry(Rect{0, 0, 512, 512});
    near(f.stroke, 13.0f, "stroke is 26/1024 of the side");
    near(f.tab, 90.0f, "tab is 180/1024 of the side");
    near(f.outerRadius, 40.0f, "corner is 80/1024 of the side");
    near(f.artRadius, 46.08f, "art is clipped at 9% of the side");
}

void glyph() {
    // home-grid.md §3.7 step 4: 45/1024 in, 90/1024 square, which is the tab's centre.
    const opensu::ui::FrameGeometry f = opensu::ui::frameGeometry(Rect{100, 200, 512, 512});
    near(f.glyph.x, 100.0f + 22.5f, "glyph square starts 45/1024 in");
    near(f.glyph.y, 200.0f + 22.5f, "glyph square starts 45/1024 down");
    near(f.glyph.width, 45.0f, "glyph square is 90/1024 of the side");
    near(f.glyph.height, 45.0f, "glyph square is square");
    near(f.glyph.centreX(), 100.0f + f.tab * 0.5f,
         "glyph square is centred in the tab horizontally");
    near(f.glyph.centreY(), 200.0f + f.tab * 0.5f, "glyph square is centred in the tab vertically");
}

void runnerBadge() {
    const Rect content{100, 200, 200, 300};
    const Rect badge = opensu::ui::runnerBadge(content);
    near(badge.width, 30.0f, "a badge is 15% of the short side");
    near(badge.height, 30.0f, "a badge is round");
    near(badge.right(), content.right() - 10.0f, "a badge is 5% in from the right");
    near(badge.bottom(), content.bottom() - 10.0f, "a badge is 5% up from the bottom");
}

void updateBadge() {
    const Rect content{100, 200, 200, 300};
    const Rect badge = opensu::ui::updateBadge(content);
    const Rect runner = opensu::ui::runnerBadge(content);
    near(badge.width, runner.width, "the update badge is a runner badge's size");
    near(badge.right(), runner.right(), "it lines up with the runner badge's right edge");
    near(badge.y, content.y + 10.0f, "it is 5% down from the top");
    opensu::test::expect(badge.bottom() < runner.y, "it clears the runner badge");
}

} // namespace

int main() try {
    outerForContent();
    cell200();
    smallTile();
    frame();
    glyph();
    runnerBadge();
    updateBadge();
    std::printf("tile_geometry: all checks passed\n");
    return 0;
} catch (...) {
    return opensu::test::escaped();
}
