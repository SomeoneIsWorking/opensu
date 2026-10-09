#include "quick_menu.hpp"

#include <algorithm>

namespace opensu::ui {
namespace {

constexpr float panelWidthDp = 380.0f;
constexpr float paddingDp = 16.0f;
constexpr float rowHeightDp = 54.0f;
constexpr float rowGapDp = 4.0f;

} // namespace

std::optional<std::size_t> QuickLayout::rowAt(float x, float y) const noexcept {
    if (!content.contains(x, y)) {
        return std::nullopt;
    }
    for (std::size_t i = 0; i < rows.size(); ++i) {
        if (rows[i].rect.contains(x, y)) {
            return i;
        }
    }
    return std::nullopt;
}

std::optional<int> QuickLayout::levelAt(const SettingsRow& row, std::size_t index, float x,
                                        float y) const noexcept {
    if (index >= rows.size() || !content.contains(x, y)) {
        return std::nullopt;
    }
    return levelOnTrack(row, rows[index], x, y);
}

QuickLayout layoutQuick(const PanelFrame& frame, const PanelChrome& chrome,
                        const std::vector<SettingsRow>& rows, std::size_t focused) {
    const auto [width, height, dp] = frame;
    const auto [headingBox, footer] = chrome;
    QuickLayout layout;
    const float panel = std::min(panelWidthDp * dp, width);
    layout.panel = Rect{width - panel, 0.0f, panel, height};
    layout.padding = paddingDp * dp;
    layout.heading = Rect{layout.panel.x + layout.padding, layout.padding,
                          panel - 2.0f * layout.padding, headingBox};
    const float top = layout.padding * 2.0f + headingBox;
    const float rowHeight = rowHeightDp * dp;
    const float rowGap = rowGapDp * dp;
    const float visible = std::max(height - top - footer - layout.padding, 0.0f);
    layout.content = Rect{layout.panel.x, top, panel, visible};
    const float contentHeight =
        static_cast<float>(rows.size()) * (rowHeight + rowGap) - (rows.empty() ? 0.0f : rowGap);
    float scroll = 0.0f;
    if (focused < rows.size()) {
        const float bottom = static_cast<float>(focused + 1) * (rowHeight + rowGap) - rowGap;
        scroll = std::max(bottom - visible, 0.0f);
    }
    scroll = std::min(scroll, std::max(contentHeight - visible, 0.0f));
    float y = top - scroll;
    for (const SettingsRow& row : rows) {
        layout.rows.push_back(settingsRowBox(
            Rect{layout.panel.x + layout.padding, y, panel - 2.0f * layout.padding, rowHeight},
            row.kind, dp));
        y += rowHeight + rowGap;
    }
    return layout;
}

void QuickMenu::open(std::vector<SettingsRow> rows) {
    rows_ = std::move(rows);
    focus_ = 0;
    open_ = true;
}

void QuickMenu::refresh(std::vector<SettingsRow> rows) {
    if (!open_) {
        return;
    }
    const std::string focused = focusedRow() != nullptr ? focusedRow()->id : std::string{};
    rows_ = std::move(rows);
    const auto same = std::ranges::find(rows_, focused, &SettingsRow::id);
    if (same != rows_.end()) {
        focus_ = static_cast<std::size_t>(same - rows_.begin());
    } else {
        focus_ = std::min(focus_, rows_.empty() ? std::size_t{0} : rows_.size() - 1);
    }
}

const SettingsRow* QuickMenu::focusedRow() const {
    return focus_ < rows_.size() ? &rows_[focus_] : nullptr;
}

bool QuickMenu::move(int delta) noexcept {
    if (rows_.empty()) {
        return false;
    }
    const auto last = static_cast<long>(rows_.size()) - 1;
    const auto next =
        static_cast<std::size_t>(std::clamp(static_cast<long>(focus_) + delta, 0L, last));
    const bool moved = next != focus_;
    focus_ = next;
    return moved;
}

bool QuickMenu::focusRow(std::size_t index) noexcept {
    if (index >= rows_.size() || index == focus_) {
        return false;
    }
    focus_ = index;
    return true;
}

} // namespace opensu::ui
