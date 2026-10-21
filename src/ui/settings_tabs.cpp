#include "settings_tabs.hpp"

#include <algorithm>

#include "quick_menu.hpp"

namespace opensu::ui {
namespace {

constexpr float stripGapDp = 10.0f;
constexpr float stripHeightDp = 34.0f;
constexpr float shoulderRoomDp = 34.0f;
constexpr float tabGapDp = 4.0f;
/// Room each side of a tab's label.
constexpr float tabPaddingDp = 8.0f;
constexpr float checkboxDp = 22.0f;
constexpr float checkboxInsetDp = 14.0f;
/// The value's share of a row, from its right edge.
constexpr float valueShare = 0.46f;

} // namespace

std::optional<std::size_t> TabsLayout::tabAt(float x, float y) const noexcept {
    for (std::size_t i = 0; i < tabs.size(); ++i) {
        if (tabs[i].contains(x, y)) {
            return i;
        }
    }
    return std::nullopt;
}

std::optional<std::size_t> TabsLayout::rowAt(float x, float y) const noexcept {
    if (!content.contains(x, y)) {
        return std::nullopt;
    }
    for (std::size_t i = 0; i < rows.size(); ++i) {
        if (rows[i].contains(x, y)) {
            return i;
        }
    }
    return std::nullopt;
}

TabColumn TabsLayout::columnAt(std::size_t index, float x, float y) const noexcept {
    if (index < checkboxes.size() && checkboxes[index].width > 0.0f) {
        // The checkbox's whole left end, not only its square, so a finger finds it.
        const Rect& box = checkboxes[index];
        const Rect& row = rows[index];
        if (Rect{row.x, row.y, box.right() + checkboxInsetDp * 0.5f - row.x, row.height}.contains(
                x, y)) {
            return TabColumn::Override;
        }
    }
    return TabColumn::Value;
}

TabsLayout layoutTabs(const PanelFrame& frame, const PanelChrome& chrome, const TabsView& view,
                      std::size_t focused, const std::vector<float>& labels) {
    const float dp = frame.dp;
    const float strip = stripHeightDp * dp;
    const float above = chrome.heading + stripGapDp * dp + strip;
    std::vector<SettingsRow> rows;
    rows.reserve(view.rows.size());
    for (const TabRow& row : view.rows) {
        rows.push_back(row.row);
    }
    const QuickLayout quick = layoutQuick(frame, PanelChrome{above, chrome.footer}, rows, focused);
    TabsLayout layout;
    layout.panel = quick.panel;
    layout.padding = quick.padding;
    layout.heading = Rect{quick.heading.x, quick.heading.y, quick.heading.width, chrome.heading};
    layout.strip =
        Rect{quick.heading.x, quick.heading.bottom() - strip, quick.heading.width, strip};
    layout.content = quick.content;
    const float shoulder = shoulderRoomDp * dp;
    const float across = std::max(layout.strip.width - 2.0f * shoulder, 0.0f);
    const auto count = static_cast<float>(view.tabs.size());
    const float gap = tabGapDp * dp;
    const float room = std::max(across - gap * std::max(count - 1.0f, 0.0f), 0.0f);
    std::vector<float> widths;
    float wanted = 0.0f;
    for (std::size_t i = 0; i < view.tabs.size(); ++i) {
        widths.push_back((i < labels.size() ? labels[i] : 0.0f) + 2.0f * tabPaddingDp * dp);
        wanted += widths.back();
    }
    const float share = count > 0.0f ? (room - wanted) / count : 0.0f;
    float x = layout.strip.x + shoulder;
    for (const float width : widths) {
        const float cell = std::max(width + share, 0.0f);
        layout.tabs.push_back(Rect{x, layout.strip.y, cell, strip});
        x += cell + gap;
    }
    for (std::size_t i = 0; i < quick.rows.size(); ++i) {
        const Rect& row = quick.rows[i].rect;
        layout.rows.push_back(row);
        const bool box = i < view.rows.size() && view.rows[i].overridable;
        layout.checkboxes.push_back(box ? Rect{row.x + checkboxInsetDp * dp,
                                               row.centreY() - checkboxDp * dp * 0.5f,
                                               checkboxDp * dp, checkboxDp * dp}
                                        : Rect{});
        const float value = row.width * valueShare;
        layout.values.push_back(Rect{row.right() - value, row.y, value, row.height});
    }
    return layout;
}

void SettingsTabs::open(TabsView view) {
    view_ = std::move(view);
    focus_ = 0;
    column_ = TabColumn::Override;
    open_ = true;
}

void SettingsTabs::refresh(TabsView view) {
    if (!open_) {
        return;
    }
    const bool sameTab = view.tab == view_.tab && view.tabs == view_.tabs;
    const std::string focused = focusedRow() != nullptr ? focusedRow()->row.id : std::string{};
    view_ = std::move(view);
    if (!sameTab) {
        focus_ = 0;
        return;
    }
    const auto same = std::ranges::find_if(view_.rows, [&focused](const TabRow& row) {
        return row.row.id == focused;
    });
    if (same != view_.rows.end()) {
        focus_ = static_cast<std::size_t>(same - view_.rows.begin());
    } else {
        focus_ = std::min(focus_, view_.rows.empty() ? std::size_t{0} : view_.rows.size() - 1);
    }
}

TabColumn SettingsTabs::column() const noexcept {
    const TabRow* row = focusedRow();
    if (row == nullptr || !row->overridable) {
        return TabColumn::Value;
    }
    return row->overridden ? column_ : TabColumn::Override;
}

const TabRow* SettingsTabs::focusedRow() const {
    return focus_ < view_.rows.size() ? &view_.rows[focus_] : nullptr;
}

bool SettingsTabs::move(int delta) noexcept {
    if (view_.rows.empty()) {
        return false;
    }
    const auto last = static_cast<long>(view_.rows.size()) - 1;
    const auto next =
        static_cast<std::size_t>(std::clamp(static_cast<long>(focus_) + delta, 0L, last));
    const bool moved = next != focus_;
    focus_ = next;
    return moved;
}

bool SettingsTabs::moveColumn(TabColumn column) noexcept {
    const TabColumn before = this->column();
    column_ = column;
    return this->column() != before;
}

bool SettingsTabs::focusRow(std::size_t index, TabColumn column) noexcept {
    if (index >= view_.rows.size()) {
        return false;
    }
    const std::size_t before = focus_;
    const TabColumn was = this->column();
    focus_ = index;
    column_ = column;
    return focus_ != before || this->column() != was;
}

} // namespace opensu::ui
