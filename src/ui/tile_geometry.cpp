#include "tile_geometry.hpp"

#include <algorithm>
#include <cmath>

namespace iideck::ui {
namespace {

// Measured on iiSU's 1024 px border sprites: 26 px stroke, 80 px outer and tab corners,
// 180 px tab.
constexpr float spriteSize = 1024.0f;
constexpr float spriteStroke = 26.0f;
constexpr float spriteCorner = 80.0f;
constexpr float spriteTab = 180.0f;
// iiSU border_pack.json roundRadiusPct 6.25; g24.e clips art at max(pct, 9).
constexpr float packRoundRadiusPct = 6.25f;
constexpr float artMinRadiusPct = 9.0f;
// g24.e: the logo's square sits at 45/1024 with side 90/1024, the tab's centre.
constexpr float spriteGlyphOffset = 45.0f;
constexpr float spriteGlyphSide = 90.0f;

// iideck's own: a store badge is 15% of the content's short side, 5% from its corner, with a
// fifth of a badge between neighbours.
constexpr float storeBadgeSide = 0.15f;
constexpr float storeBadgeMargin = 0.05f;
constexpr float storeBadgeGap = 0.2f;

/// iiSU xj2: max(4, ceil(0.068 x)).
float cornerFor(float side) noexcept {
    return std::max(4.0f, std::ceil(0.068f * side));
}

Rect inset(const Rect& rect, float by) noexcept {
    return Rect{rect.x + by, rect.y + by, std::max(rect.width - by * 2.0f, 0.0f),
                std::max(rect.height - by * 2.0f, 0.0f)};
}

} // namespace

TileGeometry tileGeometry(const Rect& rect, float cell) noexcept {
    const float shortSide = std::min(rect.width, rect.height);
    TileGeometry out;
    out.outer = rect;
    // iiSU tj2.V.
    out.frameWidth = std::max(1.5f, std::min(9.0f, 0.05f * shortSide));
    const float thick = std::max(1.5f, std::min(9.0f, 0.115f * shortSide));
    out.contentRadius =
        std::min(cornerFor(cell), std::max(4.0f, (cell - 2.0f * thick) / 2.0f - 1.0f));
    out.outerRadius = std::min(thick + out.contentRadius, std::max(6.0f, cell / 2.0f - 1.0f));
    out.content = inset(rect, out.frameWidth);
    out.outerStroke = std::max(1.0f, std::min(2.25f, 0.25f * out.frameWidth));
    out.innerShadowWidth = out.frameWidth;
    return out;
}

FrameGeometry frameGeometry(const Rect& rect) noexcept {
    const float side = std::min(rect.width, rect.height);
    const float scale = side / spriteSize;
    return FrameGeometry{
        spriteStroke * scale,
        spriteCorner * scale,
        spriteTab * scale,
        spriteCorner * scale,
        side * std::max(packRoundRadiusPct, artMinRadiusPct) / 100.0f,
        Rect{rect.x + spriteGlyphOffset * scale, rect.y + spriteGlyphOffset * scale,
             spriteGlyphSide * scale, spriteGlyphSide * scale},
    };
}

StoreIconRow storeIconRow(const Rect& content, std::size_t count) noexcept {
    StoreIconRow row;
    row.count = std::min(count, maxStoreIcons);
    const float side = std::min(content.width, content.height);
    const float badge = side * storeBadgeSide;
    const float margin = side * storeBadgeMargin;
    const float pitch = badge * (1.0f + storeBadgeGap);
    const float y = content.bottom() - margin - badge;
    for (std::size_t i = 0; i < row.count; ++i) {
        const float fromCorner = static_cast<float>(row.count - 1 - i);
        row.badges[i] = Rect{content.right() - margin - badge - fromCorner * pitch, y, badge, badge};
    }
    return row;
}

Rect containFit(float width, float height, const Rect& slot) noexcept {
    if (width <= 0.0f || height <= 0.0f) {
        return Rect{slot.centreX(), slot.centreY(), 0.0f, 0.0f};
    }
    const float fit = std::min(slot.width / width, slot.height / height);
    const float w = width * fit;
    const float h = height * fit;
    return Rect{slot.centreX() - w * 0.5f, slot.centreY() - h * 0.5f, w, h};
}

} // namespace iideck::ui
