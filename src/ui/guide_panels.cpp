#include "guide_panels.hpp"

#include <variant>

namespace opensu::ui {

void GuidePanels::draw(Vector2 size, float dp) const {
    guidePainter_.paint(guide_, size.x, size.y, dp);
    quickPainter_.paint(quick_, size.x, size.y, dp);
}

PointerTarget GuidePanels::pointAt(Vector2 point, Vector2 size, float dp) const {
    if (guide_.isOpen()) {
        const GuideLayout layout = guidePainter_.layout(guide_, size.x, size.y, dp);
        if (const std::optional<std::size_t> row = layout.rowAt(point.x, point.y)) {
            return OnGuideEntry{*row};
        }
        return layout.panel.contains(point.x, point.y) ? PointerTarget{} : OnBackdrop{};
    }
    if (quick_.isOpen()) {
        const QuickLayout layout = quickPainter_.layout(quick_, size.x, size.y, dp);
        if (const std::optional<std::size_t> row = layout.rowAt(point.x, point.y)) {
            if (const std::optional<int> level =
                    layout.levelAt(quick_.rows()[*row], *row, point.x, point.y)) {
                return OnQuickSlider{*row, *level};
            }
            return OnQuickRow{*row};
        }
        return layout.panel.contains(point.x, point.y) ? PointerTarget{} : OnBackdrop{};
    }
    return {};
}

bool GuidePanels::focusTarget(const PointerTarget& target) {
    if (const auto* entry = std::get_if<OnGuideEntry>(&target)) {
        return guide_.focusEntry(entry->index);
    }
    if (const auto* row = std::get_if<OnQuickRow>(&target)) {
        return quick_.focusRow(row->index);
    }
    if (const auto* slider = std::get_if<OnQuickSlider>(&target)) {
        return quick_.focusRow(slider->row);
    }
    return false;
}

} // namespace opensu::ui
