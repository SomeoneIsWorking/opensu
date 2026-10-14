#include "downloads_panel.hpp"

#include <variant>

namespace opensu::ui {
namespace {

/// The frame between the top and bottom bars.
Rect roomOf(Point size, float topInset, float bottomInset) {
    return Rect{0.0f, topInset, size.x, size.y - topInset - bottomInset};
}

} // namespace

void DownloadsPanel::draw(Canvas& canvas, Point size, float dp, float topInset, float bottomInset,
                          Clock::time_point now) const {
    if (!fade_.visible(now)) {
        return;
    }
    painter_.paint(canvas, page_, page_.layout(roomOf(size, topInset, bottomInset), dp), size, dp,
                   fade_.alpha(now));
}

PointerTarget DownloadsPanel::pointAt(Point point, Point size, float dp, float topInset,
                                      float bottomInset) const {
    const DownloadsLayout layout = page_.layout(roomOf(size, topInset, bottomInset), dp);
    if (const auto action = layout.actionAt(point.x, point.y)) {
        return OnDownloadAction{action->first, action->second};
    }
    if (const std::optional<std::size_t> row = layout.rowAt(point.x, point.y)) {
        return OnDownloadRow{*row};
    }
    return {};
}

bool DownloadsPanel::focusTarget(const PointerTarget& target) {
    if (const auto* action = std::get_if<OnDownloadAction>(&target)) {
        return page_.focusAction(action->row, action->action);
    }
    if (const auto* row = std::get_if<OnDownloadRow>(&target)) {
        return page_.focusRow(row->row);
    }
    return false;
}

} // namespace opensu::ui
