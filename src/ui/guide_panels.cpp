#include "guide_panels.hpp"

#include <variant>

namespace opensu::ui {

void GuidePanels::draw(Canvas& canvas, Point size, float dp) const {
    guidePainter_.paint(canvas, guide_, size.x, size.y, dp);
    quickPainter_.paint(canvas, quick_, size.x, size.y, dp);
    tabsPainter_.paint(canvas, tabs_, size.x, size.y, dp);
}

PointerTarget GuidePanels::pointAt(Point point, Point size, float dp) const {
    if (tabs_.isOpen()) {
        const TabsLayout layout = tabsPainter_.layout(tabs_, size.x, size.y, dp);
        if (const std::optional<std::size_t> tab = layout.tabAt(point.x, point.y)) {
            return OnSettingsTab{*tab};
        }
        if (const std::optional<std::size_t> row = layout.rowAt(point.x, point.y)) {
            return OnTabRow{*row, layout.columnAt(*row, point.x, point.y) == TabColumn::Override};
        }
        return layout.panel.contains(point.x, point.y) ? PointerTarget{} : OnBackdrop{};
    }
    if (guide_.isOpen()) {
        const GuideLayout layout = guidePainter_.layout(guide_, size.x, size.y, dp);
        if (const std::optional<std::size_t> row = layout.rowAt(point.x, point.y)) {
            return OnGuideEntry{*row};
        }
        if (!guide_.inPower() && layout.powerButton.contains(point.x, point.y)) {
            return OnGuidePower{};
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
    if (std::holds_alternative<OnGuidePower>(target)) {
        return guide_.focusPower();
    }
    if (const auto* row = std::get_if<OnQuickRow>(&target)) {
        return quick_.focusRow(row->index);
    }
    if (const auto* slider = std::get_if<OnQuickSlider>(&target)) {
        return quick_.focusRow(slider->row);
    }
    if (const auto* row = std::get_if<OnTabRow>(&target)) {
        return tabs_.focusRow(row->index, row->checkbox ? TabColumn::Override : TabColumn::Value);
    }
    return false;
}

} // namespace opensu::ui
