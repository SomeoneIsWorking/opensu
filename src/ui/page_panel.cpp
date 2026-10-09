#include "page_panel.hpp"

#include <variant>

#include "settings_page_painter.hpp"

namespace opensu::ui {

void PagePanel::draw(Vector2 size, float dp, float topInset, float bottomInset,
                     Clock::time_point now) const {
    // A page that has not been opened has no categories to lay out.
    if (!fade_.visible(now)) {
        return;
    }
    SettingsPagePainter::paint(page_, page_.layout(size.x, size.y, dp, topInset, bottomInset), size,
                               dp, fade_.alpha(now));
}

PointerTarget PagePanel::pointAt(Vector2 point, Vector2 size, float dp, float topInset,
                                 float bottomInset) const {
    const SettingsLayout layout = page_.layout(size.x, size.y, dp, topInset, bottomInset);
    if (const std::optional<std::size_t> category = layout.categoryAt(point.x, point.y)) {
        return OnSettingsCategory{*category};
    }
    if (const std::optional<std::size_t> row = layout.rowAt(point.x, point.y)) {
        const SettingsRow& shown = page_.focusedCategory().rows[*row];
        if (const std::optional<int> level = layout.levelAt(shown, *row, point.x, point.y)) {
            return OnSettingsSlider{*row, *level};
        }
        return OnSettingsRow{*row};
    }
    return {};
}

bool PagePanel::focusTarget(const PointerTarget& target) {
    if (const auto* category = std::get_if<OnSettingsCategory>(&target)) {
        return page_.focusCategory(category->index);
    }
    if (const auto* row = std::get_if<OnSettingsRow>(&target)) {
        return page_.focusRow(row->index);
    }
    if (const auto* slider = std::get_if<OnSettingsSlider>(&target)) {
        return page_.focusRow(slider->row);
    }
    return false;
}

} // namespace opensu::ui
