#include "dock_painter.hpp"

#include <algorithm>
#include <cmath>

#include "dock_motion.hpp"
#include "round_shape.hpp"

namespace opensu::ui {
namespace {

// The badges' ink: the corner hints' (iiSU bell_icon.png).
constexpr Colour badgeInk{0x4D, 0x46, 0x55, 255};

Rect shifted(const Rect& rect, float dy) noexcept {
    return Rect{rect.x, rect.y + dy, rect.width, rect.height};
}

} // namespace

float DockPainter::slideOffset(const DockLayout& layout, const DockStyle& style) noexcept {
    // navigation.md §5.1: the bar slides straight down through the bottom edge as it fades.
    const float progress = std::clamp(style.progress, 0.0f, 1.0f);
    return (1.0f - progress) * (layout.bar.height + DockMetrics::bottomGap * style.dp);
}

Rect DockPainter::barRect(const DockLayout& layout, const DockStyle& style) noexcept {
    return shifted(layout.bar, slideOffset(layout, style));
}

float DockPainter::top(const DockLayout& layout, const DockStyle& style) noexcept {
    return std::min({layout.bar.y, layout.leftBadge.y, layout.rightBadge.y}) +
           slideOffset(layout, style);
}

void DockPainter::paint(Canvas& canvas, const DockLayout& layout, const DockMetrics& metrics,
                        std::span<const DockIcon> icons, const DockStyle& style) {
    const float alpha = std::clamp(style.progress, 0.0f, 1.0f);
    if (alpha <= 0.0f) {
        return;
    }
    const float dy = slideOffset(layout, style);
    glass_.paintPill(canvas, shifted(layout.bar, dy),
                     PillStyle{style.dark, metrics.borderWidth * style.dp, style.dp, alpha});

    for (std::size_t i = 0; i < layout.icons.size() && i < icons.size(); ++i) {
        const Rect box = shifted(layout.icons[i], dy);
        const DockIcon& icon = icons[i];
        if (icon.hovered) {
            const Rect item = shifted(layout.items[i], dy);
            canvas.fillRoundRect(onCanvas(item), cornerRadius(item, 1.0f),
                                 withAlpha(badgeInk, 0.12f * alpha));
        }
        // Rasterised at the size it rests at, so a resting icon maps texel to pixel.
        const float rest = icon.selected ? IconPop::restSelected : 1.0f;
        const int width = std::max(1, static_cast<int>(std::lround(box.width * rest)));
        const int height = std::max(1, static_cast<int>(std::lround(box.height * rest)));
        const gpu2d::Texture* texture =
            icons_.image(dockIcon(icon.section, icon.selected), width, height);
        if (texture == nullptr) {
            continue;
        }
        const float grow = icon.scale / rest;
        const float drawnWidth = static_cast<float>(width) * grow;
        const float drawnHeight = static_cast<float>(height) * grow;
        canvas.image(texture->view(), wholeOf(*texture),
                     gpu2d::Rect{std::round(box.centreX() - drawnWidth * 0.5f),
                                 std::round(box.centreY() - drawnHeight * 0.5f), drawnWidth,
                                 drawnHeight},
                     withAlpha(white, alpha));
    }

    const Colour ink = withAlpha(badgeInk, alpha);
    const Rect left = shifted(layout.leftBadge, dy);
    const Rect right = shifted(layout.rightBadge, dy);
    glyphs_.paint(canvas, "LB", Point{left.centreX(), left.centreY()}, left.width, ink);
    glyphs_.paint(canvas, "RB", Point{right.centreX(), right.centreY()}, right.width, ink);
}

} // namespace opensu::ui
