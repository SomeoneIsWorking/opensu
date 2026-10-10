#include "breadcrumb_painter.hpp"

#include <algorithm>
#include <string>

#include "raylib.h"

#include "typeface.hpp"

namespace opensu::ui {
namespace {

// The ink the hint glyphs and the title pill share, and a softer one for the levels above.
constexpr Color currentInk{0x4D, 0x46, 0x55, 255};
constexpr Color pastInk{0x4D, 0x46, 0x55, 170};
constexpr Color hoverFill{0x2B, 0x27, 0x33, 31};

} // namespace

BreadcrumbLayout BreadcrumbPainter::layout(const Trail& trail, const BreadcrumbFrame& frame,
                                           const TitlePillMetrics& text) const {
    const TextStyle style{text.fontSize * frame.dp};
    std::vector<float> widths;
    widths.reserve(trail.size());
    for (const Crumb& crumb : trail) {
        widths.push_back(typeface_.measure(crumb.label, style));
    }
    return layoutBreadcrumbs(frame, widths);
}

void BreadcrumbPainter::paint(const Trail& trail, const BreadcrumbLayout& layout,
                              const TitlePillMetrics& text, float dp,
                              std::optional<std::size_t> hovered) const {
    if (trail.empty() || layout.cells.size() != trail.size()) {
        return;
    }
    glass_.paint(layout.bar);
    const TextStyle style{text.fontSize * dp};
    for (std::size_t i = 0; i < trail.size(); ++i) {
        const Rect& cell = layout.cells[i];
        const bool last = i + 1 == trail.size();
        if (hovered == i && isPlace(trail[i])) {
            DrawRectangleRounded(
                Rectangle{cell.x, cell.y + cell.height * 0.14f, cell.width, cell.height * 0.72f},
                0.5f, 12, hoverFill);
        }
        const std::string label = typeface_.fitted(trail[i].label, layout.textWidths[i], style);
        typeface_.drawCentred(label, cell.x + crumbCellPadDp * dp, cell.centreY(), style,
                              last ? currentInk : pastInk);
        if (!last) {
            const float x = layout.chevrons[i];
            const float half = cell.height * 0.1f;
            const float y = cell.centreY();
            const float line = std::max(dp * 1.4f, 1.5f);
            DrawLineEx(Vector2{x - half * 0.5f, y - half}, Vector2{x + half * 0.5f, y}, line,
                       pastInk);
            DrawLineEx(Vector2{x + half * 0.5f, y}, Vector2{x - half * 0.5f, y + half}, line,
                       pastInk);
        }
    }
}

} // namespace opensu::ui
