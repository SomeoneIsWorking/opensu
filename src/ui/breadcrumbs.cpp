#include "breadcrumbs.hpp"

#include <algorithm>

namespace opensu::ui {

bool isPlace(const Crumb& crumb) noexcept {
    return crumb.kind != CrumbKind::Filter && crumb.kind != CrumbKind::Game;
}

std::optional<std::size_t> BreadcrumbLayout::crumbAt(float x, float y) const noexcept {
    for (std::size_t i = 0; i < cells.size(); ++i) {
        if (cells[i].contains(x, y)) {
            return i;
        }
    }
    return std::nullopt;
}

BreadcrumbLayout layoutBreadcrumbs(const BreadcrumbFrame& frame,
                                   const std::vector<float>& textWidths) {
    BreadcrumbLayout layout;
    layout.textWidths = textWidths;
    const float dp = frame.dp;
    const float pad = crumbPadDp * dp;
    const float cellPad = crumbCellPadDp * dp;
    const float chevron = crumbChevronDp * dp;
    const auto count = static_cast<float>(textWidths.size());
    const float fixed =
        2.0f * pad + count * 2.0f * cellPad + std::max(count - 1.0f, 0.0f) * chevron;
    const float room = std::max(frame.maxRight - frame.left, fixed);
    const float floor = crumbMinTextDp * dp;
    float excess = fixed;
    for (const float width : layout.textWidths) {
        excess += width;
    }
    excess -= room;
    // The widest labels shrink first, down to the next widest, so short ones stay whole.
    while (excess > 0.5f) {
        const auto widest = std::ranges::max_element(layout.textWidths);
        if (widest == layout.textWidths.end() || *widest <= floor) {
            break;
        }
        const float top = *widest;
        float next = floor;
        for (const float width : layout.textWidths) {
            if (width < top) {
                next = std::max(next, width);
            }
        }
        const auto tied = static_cast<float>(std::ranges::count(layout.textWidths, top));
        const float step = std::min((top - next) * tied, excess);
        std::ranges::replace(layout.textWidths, top, top - step / tied);
        excess -= step;
    }

    float x = frame.left + pad;
    for (std::size_t i = 0; i < layout.textWidths.size(); ++i) {
        const float width = layout.textWidths[i] + 2.0f * cellPad;
        layout.cells.push_back(Rect{x, frame.top, width, frame.height});
        x += width;
        if (i + 1 < layout.textWidths.size()) {
            layout.chevrons.push_back(x + chevron * 0.5f);
            x += chevron;
        }
    }
    layout.bar = Rect{frame.left, frame.top, x + pad - frame.left, frame.height};
    return layout;
}

} // namespace opensu::ui
