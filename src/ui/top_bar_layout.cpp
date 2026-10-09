#include "top_bar_layout.hpp"

#include <algorithm>

namespace opensu::ui {
namespace {

// Space inside the pill at the launcher column's ends, and between badges as a share of one.
constexpr float columnPaddingDp = 8.0f;
constexpr float badgeGap = 0.55f;
// A badge is this share of the pill's height: inline with the clock's text, not a button of its
// own.
constexpr float badgeOfPill = 0.52f;
// The clock's area starts this far after the divider, in dp.
constexpr float dividerGapDp = 2.0f;
// A hit extends this far past the circle, so the gap between two badges belongs to one.
constexpr float hitSlopDp = 2.0f;

} // namespace

TopBarLayout layoutTopBar(const TopBarMetrics& metrics, const TopBarFrame& frame) {
    TopBarLayout layout;
    const float dp = frame.dp;
    layout.pill = metrics.statusPill(frame.clockHasLetters);
    layout.top = TopBarMetrics::rowPaddingTop * dp;
    layout.hitSlop = hitSlopDp * dp;
    const float diameter = layout.pill.height * badgeOfPill * dp;
    const float gap = diameter * badgeGap;
    const auto count = static_cast<float>(frame.launchers);
    layout.launcherColumn = frame.launchers == 0 ? 0.0f
                                                 : 2.0f * columnPaddingDp * dp + count * diameter +
                                                       (count - 1.0f) * gap;

    const float aspect =
        std::max(frame.width, frame.height) / std::max(std::min(frame.width, frame.height), 1.0f);
    // The pill ends at the row's end padding and is offset by the device class rule.
    const float boxRight =
        frame.width - TopBarMetrics::rowPaddingEnd * dp + metrics.statusOffsetX(aspect) * dp;
    const float width =
        layout.launcherColumn + frame.contentWidth + 2.0f * layout.pill.paddingHorizontal * dp;
    layout.status = Rect{boxRight - metrics.sizing().endPadding * dp - width, layout.top, width,
                         layout.pill.height * dp};

    if (frame.launchers != 0) {
        layout.divider = layout.status.x + layout.launcherColumn - dividerGapDp * dp;
    }
    float x = layout.status.x + columnPaddingDp * dp;
    for (std::size_t i = 0; i < frame.launchers; ++i) {
        layout.launchers.push_back(
            Rect{x, layout.status.centreY() - diameter * 0.5f, diameter, diameter});
        x += diameter + gap;
    }
    return layout;
}

std::optional<std::size_t> TopBarLayout::launcherAt(float x, float y) const noexcept {
    for (std::size_t i = 0; i < launchers.size(); ++i) {
        const Rect& cell = launchers[i];
        const Rect hit{cell.x - hitSlop, cell.y - hitSlop, cell.width + 2.0f * hitSlop,
                       cell.height + 2.0f * hitSlop};
        if (hit.contains(x, y)) {
            return i;
        }
    }
    return std::nullopt;
}

} // namespace opensu::ui
