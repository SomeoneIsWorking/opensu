#include "settings_panels.hpp"

#include <variant>

#include "settings_page_painter.hpp"

namespace opensu::ui {

void SettingsPanels::tick(Clock::time_point now) noexcept {
    pageFade_.follow(page_.isOpen(), now);
    folderFade_.follow(folders_.isOpen(), now);
    entryFade_.follow(entry_.isOpen(), now);
}

void SettingsPanels::drawPage(Vector2 size, float dp, float topInset, float bottomInset,
                              Clock::time_point now) const {
    SettingsPagePainter::paint(page_, page_.layout(size.x, size.y, dp, topInset, bottomInset), size,
                               dp, pageFade_.alpha(now));
}

void SettingsPanels::drawOverlays(Vector2 size, float dp, Clock::time_point now,
                                  double seconds) const {
    folderPainter_.paint(folders_, size, dp, folderFade_.look(now));
    entryPainter_.paint(entry_, size, dp, entryFade_.look(now), seconds);
}

PointerTarget SettingsPanels::pointAt(Vector2 point, Vector2 size, float dp, float topInset,
                                      float bottomInset) const {
    const Rect frame{0.0f, 0.0f, size.x, size.y};
    if (entry_.isOpen()) {
        const SearchLayout panel = layoutSearch(frame, dp, entry_.lists());
        if (const std::optional<std::size_t> key = panel.keyAt(point.x, point.y)) {
            return OnSearchKey{*key};
        }
        return {};
    }
    if (folders_.isOpen()) {
        if (const std::optional<std::size_t> row =
                folders_.layout(frame, dp).rowAt(point.x, point.y)) {
            return OnFolderEntry{*row};
        }
        return {};
    }
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

bool SettingsPanels::focusTarget(const PointerTarget& target) {
    if (const auto* category = std::get_if<OnSettingsCategory>(&target)) {
        return page_.focusCategory(category->index);
    }
    if (const auto* row = std::get_if<OnSettingsRow>(&target)) {
        return page_.focusRow(row->index);
    }
    if (const auto* slider = std::get_if<OnSettingsSlider>(&target)) {
        return page_.focusRow(slider->row);
    }
    if (const auto* entry = std::get_if<OnFolderEntry>(&target)) {
        return folders_.focusEntry(entry->index);
    }
    if (const auto* key = std::get_if<OnSearchKey>(&target); key != nullptr && entry_.isOpen()) {
        return entry_.focusKey(key->index);
    }
    return false;
}

} // namespace opensu::ui
