#include "settings_panels.hpp"

#include <variant>

namespace opensu::ui {

void SettingsPanels::tick(Clock::time_point now) noexcept {
    folderFade_.follow(folders_.isOpen(), now);
    entryFade_.follow(entry_.isOpen(), now);
}

void SettingsPanels::drawOverlays(Canvas& canvas, Point size, float dp, Clock::time_point now,
                                  double seconds) const {
    folderPainter_.paint(canvas, folders_, folderFrame(size, dp), size, dp, folderFade_.look(now));
    entryPainter_.paint(canvas, entry_, size, dp, entryFade_.look(now), seconds);
}

Rect SettingsPanels::folderFrame(Point size, float dp) const {
    // Its "Type a path" keyboard docks under it, so it stands above the dock even before that.
    return Rect{0.0f, 0.0f, size.x,
                layoutKeyboard(Rect{0.0f, 0.0f, size.x, size.y}, dp, entry_.style()).panel.y};
}

PointerTarget SettingsPanels::pointAt(Point point, Point size, float dp) const {
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
                folders_.layout(folderFrame(size, dp), dp).rowAt(point.x, point.y)) {
            return OnFolderEntry{*row};
        }
    }
    return {};
}

bool SettingsPanels::focusTarget(const PointerTarget& target) {
    if (const auto* entry = std::get_if<OnFolderEntry>(&target)) {
        return folders_.focusEntry(entry->index);
    }
    if (const auto* key = std::get_if<OnKeyboardKey>(&target); key != nullptr && entry_.isOpen()) {
        return entry_.focusKey(key->index);
    }
    return false;
}

} // namespace opensu::ui
