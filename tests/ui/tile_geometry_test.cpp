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

} // namespace

int main() {
    cell200();
    smallTile();
    frame();
    std::printf("tile_geometry: all checks passed\n");
    return 0;
}
