#include "settings_panels.hpp"

#include <variant>

namespace opensu::ui {

void SettingsPanels::tick(Clock::time_point now) noexcept {
    page_.tick(now);
    folderFade_.follow(folders_.isOpen(), now);
    entryFade_.follow(entry_.isOpen(), now);
}

void SettingsPanels::drawPage(Canvas& canvas, Point size, float dp, float topInset,
                              float bottomInset, Clock::time_point now) const {
    page_.draw(canvas, size, dp, topInset, bottomInset, now);
}

void SettingsPanels::drawOverlays(Canvas& canvas, Point size, float dp, Clock::time_point now,
                                  double seconds) const {
    folderPainter_.paint(canvas, folders_, size, dp, folderFade_.look(now));
    entryPainter_.paint(canvas, entry_, size, dp, entryFade_.look(now), seconds);
}

PointerTarget SettingsPanels::pointAt(Point point, Point size, float dp, float topInset,
                                      float bottomInset) const {
    const Rect frame{0.0f, 0.0f, size.x, size.y};
    if (entry_.isOpen()) {
        const KeyboardLayout panel = layoutKeyboard(frame, dp, entry_.style());
        if (const std::optional<std::size_t> key = panel.keyAt(point.x, point.y)) {
            return OnKeyboardKey{*key};
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
    return page_.pointAt(point, size, dp, topInset, bottomInset);
}

bool SettingsPanels::focusTarget(const PointerTarget& target) {
    if (const auto* entry = std::get_if<OnFolderEntry>(&target)) {
        return folders_.focusEntry(entry->index);
    }
    if (const auto* key = std::get_if<OnKeyboardKey>(&target); key != nullptr && entry_.isOpen()) {
        return entry_.focusKey(key->index);
    }
    return page_.focusTarget(target);
}

} // namespace opensu::ui
