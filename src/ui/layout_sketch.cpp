#include "layout_sketch.hpp"

#include <algorithm>
#include <array>
#include <cstddef>

namespace opensu::ui {
namespace {

namespace tk = gpu2d::ui;

// The sketches' tiles take these borders in turn, as iiSU's console cards do.
constexpr std::array<gpu2d::Colour, 6> tileBorders{{{0xE0, 0x31, 0x5A, 255},
                                                    {0x3A, 0x87, 0xD8, 255},
                                                    {0xC8, 0x5A, 0xB8, 255},
                                                    {0x2F, 0xB9, 0xA0, 255},
                                                    {0xE8, 0x92, 0x3A, 255},
                                                    {0x7A, 0x5A, 0xD8, 255}}};
constexpr gpu2d::Colour white{255, 255, 255, 255};
constexpr float tileBorderDp = 1.4f;
// A sketched tile's corner, and the sketch's height, as shares of its side and of its width.
constexpr float tileCorner = 0.16f;
constexpr float heightShare = 0.5f;

// The sketches, as shares of the sketch's height (measured on
// `roms_layout_chooser_xmb_light.png` at 352 px): the grid's tiles and pitch, the rails' small and
// focused tiles and their gap.
constexpr float gridTile = 0.256f;
constexpr float gridPitch = 0.29f;
constexpr float gridTop = 0.085f;
constexpr float railSmall = 0.187f;
constexpr float railFocused = 0.364f;
constexpr float railGap = 0.03f;
constexpr float xmbLeft = 0.20f;
constexpr float carouselBottom = 0.79f;

void sketchTile(const tk::Painter& painter, const gpu2d::Rect& rect, std::size_t colour) {
    const gpu2d::Colour border = tileBorders[colour % tileBorders.size()];
    const float radius = std::min(rect.width, rect.height) * tileCorner;
    const float inset = painter.px(tileBorderDp);
    painter.canvas.fillRoundRect(rect, radius, painter.fade(border));
    painter.canvas.fillRoundRect(gpu2d::Rect{rect.x + inset, rect.y + inset,
                                             rect.width - 2.0f * inset, rect.height - 2.0f * inset},
                                 std::max(radius - inset, 0.0f),
                                 painter.fade(tk::mix(border, white, 0.82f)));
}

/// Three small tiles each side of a focused one along a rail through `middle`, `big` the focused
/// tile's side; `place` puts a tile of `side` at `offset` from the middle along the rail.
template <typename Place>
void sketchRail(const tk::Painter& painter, float big, float small, float gap, std::size_t first,
                Place place) {
    sketchTile(painter, place(-big * 0.5f, big), first);
    for (std::size_t step = 1; step <= 3; ++step) {
        const auto away = static_cast<float>(step);
        sketchTile(painter, place(-big * 0.5f - gap - away * small - (away - 1.0f) * gap, small),
                   first + tileBorders.size() - step);
        sketchTile(painter, place(big * 0.5f + gap + (away - 1.0f) * (small + gap), small),
                   first + step);
    }
}

} // namespace

tk::Size LayoutSketch::measure(tk::LayoutContext& /*context*/, float maxWidth) {
    return tk::Size{maxWidth, maxWidth * heightShare};
}

void LayoutSketch::paint(tk::Painter& painter) const {
    const gpu2d::Rect& area = frame_;
    const float side = area.height;
    painter.canvas.pushClip(area);
    if (mode_ == library::LibraryMode::Standard) {
        const float pitch = gridPitch * side;
        const auto columns = static_cast<std::size_t>(area.width / pitch) + 1;
        const float left = area.x + (area.width - (static_cast<float>(columns) * pitch)) * 0.5f;
        for (std::size_t column = 0; column < columns; ++column) {
            for (std::size_t row = 0; row < 3; ++row) {
                sketchTile(
                    painter,
                    gpu2d::Rect{left + static_cast<float>(column) * pitch,
                                area.y + (gridTop + static_cast<float>(row) * gridPitch) * side,
                                gridTile * side, gridTile * side},
                    column * 3 + row);
            }
        }
    } else if (mode_ == library::LibraryMode::Xmb) {
        const float left = area.x + (area.width - side) * 0.5f + xmbLeft * side;
        const float middle = area.y + side * 0.5f;
        sketchRail(painter, railFocused * side, railSmall * side, railGap * side, 3,
                   [left, middle](float offset, float tile) {
                       return gpu2d::Rect{left, middle + offset, tile, tile};
                   });
    } else {
        const float bottom = area.y + carouselBottom * side;
        const float middle = area.x + area.width * 0.5f;
        sketchRail(painter, railFocused * side, railSmall * side, railGap * side, 2,
                   [bottom, middle](float offset, float tile) {
                       return gpu2d::Rect{middle + offset, bottom - tile, tile, tile};
                   });
    }
    painter.canvas.popClip();
}

} // namespace opensu::ui
