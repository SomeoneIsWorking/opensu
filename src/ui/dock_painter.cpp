#include "dock_painter.hpp"

#include <algorithm>

#include "tile_geometry.hpp"
#include "typeface.hpp"

namespace iideck::ui {
namespace {

// The badges' ink: the corner hints' (iiSU bell_icon.png).
constexpr Color badgeInk{0x4D, 0x46, 0x55, 255};
// The stand-in icon's ink, until iiSU's own arrives.
constexpr Color initialInk{0x4D, 0x46, 0x55, 255};

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

void DockPainter::paint(const DockLayout& layout, const DockMetrics& metrics,
                        std::span<const DockIcon> icons, const DockStyle& style) {
    const float alpha = std::clamp(style.progress, 0.0f, 1.0f);
    if (alpha <= 0.0f) {
        return;
    }
    const float dy = slideOffset(layout, style);
    glass_.paintPill(shifted(layout.bar, dy),
                     PillStyle{style.dark, metrics.borderWidth * style.dp, style.dp, alpha});

    for (std::size_t i = 0; i < layout.icons.size() && i < icons.size(); ++i) {
        const Rect box = shifted(layout.icons[i], dy);
        const DockIcon& icon = icons[i];
        if (icon.hovered) {
            const Rect item = shifted(layout.items[i], dy);
            DrawRectangleRounded(Rectangle{item.x, item.y, item.width, item.height}, 1.0f, 16,
                                 Fade(badgeInk, 0.12f * alpha));
        }
        if (icon.texture != nullptr && icon.texture->id != 0) {
            const Rect fit = containFit(static_cast<float>(icon.texture->width),
                                        static_cast<float>(icon.texture->height), box);
            const float width = fit.width * icon.scale;
            const float height = fit.height * icon.scale;
            DrawTexturePro(*icon.texture,
                           Rectangle{0.0f, 0.0f, static_cast<float>(icon.texture->width),
                                     static_cast<float>(icon.texture->height)},
                           Rectangle{fit.centreX() - width * 0.5f, fit.centreY() - height * 0.5f,
                                     width, height},
                           Vector2{0.0f, 0.0f}, 0.0f, Fade(WHITE, alpha));
            continue;
        }
        const TextStyle letter{box.height * 0.6f * icon.scale};
        const float width = type().measure(icon.initial, letter);
        type().drawCentred(icon.initial, box.centreX() - width * 0.5f, box.centreY(), letter,
                           Fade(initialInk, alpha));
    }

    const Color ink = Fade(badgeInk, alpha);
    const Rect left = shifted(layout.leftBadge, dy);
    const Rect right = shifted(layout.rightBadge, dy);
    glyphs_.paint("LB", Vector2{left.centreX(), left.centreY()}, left.width, ink);
    glyphs_.paint("RB", Vector2{right.centreX(), right.centreY()}, right.width, ink);
}

} // namespace iideck::ui
