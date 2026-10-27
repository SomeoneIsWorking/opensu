#include "breadcrumb_painter.hpp"

#include <algorithm>
#include <string>

#include "gpu2d/ui/text_fit.hpp"

namespace opensu::ui {
namespace {

// The ink the hint glyphs and the title pill share, and a softer one for the levels above.
constexpr Colour currentInk{0x4D, 0x46, 0x55, 255};
constexpr Colour pastInk{0x4D, 0x46, 0x55, 170};
constexpr Colour hoverFill{0x2B, 0x27, 0x33, 31};

} // namespace

BreadcrumbLayout BreadcrumbPainter::layout(const Trail& trail, const BreadcrumbFrame& frame,
                                           const TitlePillMetrics& text) const {
    const float style = text.fontSize * frame.dp;
    std::vector<float> widths;
    widths.reserve(trail.size());
    for (const Crumb& crumb : trail) {
        widths.push_back(text_.measure(crumb.label, style));
    }
    return layoutBreadcrumbs(frame, widths);
}

void BreadcrumbPainter::paint(Canvas& canvas, const Trail& trail, const BreadcrumbLayout& layout,
                              const TitlePillMetrics& text, float dp,
                              std::optional<std::size_t> hovered) const {
    if (trail.empty() || layout.cells.size() != trail.size()) {
        return;
    }
    glass_.paint(canvas, layout.bar);
    const float style = text.fontSize * dp;
    for (std::size_t i = 0; i < trail.size(); ++i) {
        const Rect& cell = layout.cells[i];
        const bool last = i + 1 == trail.size();
        if (hovered == i && isPlace(trail[i])) {
            const Rect hover{cell.x, cell.y + cell.height * 0.14f, cell.width, cell.height * 0.72f};
            canvas.fillRoundRect(onCanvas(hover), cornerRadius(hover, 0.5f), hoverFill);
        }
        const std::string label = gpu2d::ui::fitted(text_, trail[i].label, layout.textWidths[i], style);
        text_.drawCentred(canvas, label, Point{cell.x + crumbCellPadDp * dp, cell.centreY()}, style, last ? currentInk : pastInk);
        if (!last) {
            const float x = layout.chevrons[i];
            const float half = cell.height * 0.1f;
            const float y = cell.centreY();
            const float line = std::max(dp * 1.4f, 1.5f);
            canvas.line(Point{x - half * 0.5f, y - half}, Point{x + half * 0.5f, y}, line, pastInk);
            canvas.line(Point{x + half * 0.5f, y}, Point{x - half * 0.5f, y + half}, line, pastInk);
        }
    }
}

} // namespace opensu::ui
