#include "dock_painter.hpp"

#include <algorithm>

#include "round_shape.hpp"
#include "tile_geometry.hpp"
#include "typeface.hpp"

namespace opensu::ui {
namespace {

// The badges' ink: the corner hints' (iiSU bell_icon.png).
constexpr Colour badgeInk{0x4D, 0x46, 0x55, 255};
// The stand-in icon's ink, until iiSU's own arrives.
constexpr Colour initialInk{0x4D, 0x46, 0x55, 255};

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
        if (icon.texture != nullptr) {
            const Rect fit = containFit(static_cast<float>(icon.texture->width()),
                                        static_cast<float>(icon.texture->height()), box);
            const float width = fit.width * icon.scale;
            const float height = fit.height * icon.scale;
            canvas.image(icon.texture->view(), wholeOf(*icon.texture),
                         gpu2d::Rect{fit.centreX() - width * 0.5f, fit.centreY() - height * 0.5f,
                                     width, height},
                         withAlpha(white, alpha));
            continue;
        }
        const TextStyle letter{box.height * 0.6f * icon.scale};
        const float width = typeface_.measure(icon.initial, letter);
        typeface_.drawCentred(canvas, icon.initial, box.centreX() - width * 0.5f, box.centreY(),
                              letter, withAlpha(initialInk, alpha));
    }

    const Colour ink = withAlpha(badgeInk, alpha);
    const Rect left = shifted(layout.leftBadge, dy);
    const Rect right = shifted(layout.rightBadge, dy);
    glyphs_.paint(canvas, "LB", Point{left.centreX(), left.centreY()}, left.width, ink);
    glyphs_.paint(canvas, "RB", Point{right.centreX(), right.centreY()}, right.width, ink);
}

} // namespace opensu::ui
