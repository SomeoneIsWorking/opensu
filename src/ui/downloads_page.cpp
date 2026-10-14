#include "downloads_page.hpp"

#include <algorithm>

namespace opensu::ui {
namespace {

constexpr float marginDp = 40.0f;
constexpr float panelPaddingDp = 12.0f;
constexpr float radiusDp = 16.0f;
constexpr float rowHeightDp = 76.0f;
constexpr float rowGapDp = 6.0f;
constexpr float insetDp = 16.0f;
constexpr float actionWidthDp = 124.0f;
constexpr float actionHeightDp = 34.0f;
constexpr float actionGapDp = 8.0f;
constexpr float barHeightDp = 6.0f;
constexpr float barBottomDp = 12.0f;

} // namespace

void DownloadsPage::open(std::vector<DownloadsEntry> entries) {
    entries_ = std::move(entries);
    row_ = 0;
    action_ = 0;
    open_ = true;
}

void DownloadsPage::refresh(std::vector<DownloadsEntry> entries) {
    const std::string key = row_ < entries_.size() ? entries_[row_].key : std::string{};
    entries_ = std::move(entries);
    const auto found = std::ranges::find(entries_, key, &DownloadsEntry::key);
    if (found != entries_.end()) {
        row_ = static_cast<std::size_t>(found - entries_.begin());
    } else {
        row_ = std::min(row_, entries_.empty() ? std::size_t{0} : entries_.size() - 1);
        action_ = 0;
    }
    const std::size_t buttons = row_ < entries_.size() ? entries_[row_].actions.size() : 0;
    action_ = buttons == 0 ? 0 : std::min(action_, buttons - 1);
}

const DownloadsEntry* DownloadsPage::focused() const {
    return row_ < entries_.size() ? &entries_[row_] : nullptr;
}

bool DownloadsPage::move(int delta) noexcept {
    if (entries_.empty()) {
        return false;
    }
    const auto last = static_cast<long>(entries_.size()) - 1;
    return focusRow(
        static_cast<std::size_t>(std::clamp(static_cast<long>(row_) + delta, 0L, last)));
}

bool DownloadsPage::step(int delta) noexcept {
    const DownloadsEntry* entry = focused();
    if (entry == nullptr || entry->actions.empty()) {
        return false;
    }
    const auto last = static_cast<long>(entry->actions.size()) - 1;
    return focusAction(
        row_, static_cast<std::size_t>(std::clamp(static_cast<long>(action_) + delta, 0L, last)));
}

bool DownloadsPage::focusRow(std::size_t index) noexcept {
    if (index >= entries_.size() || index == row_) {
        return false;
    }
    row_ = index;
    action_ = 0;
    return true;
}

bool DownloadsPage::focusAction(std::size_t index, std::size_t action) noexcept {
    if (index >= entries_.size() || action >= entries_[index].actions.size() ||
        (index == row_ && action == action_)) {
        return false;
    }
    row_ = index;
    action_ = action;
    return true;
}

DownloadsLayout DownloadsPage::layout(const Rect& room, float dp) const {
    DownloadsLayout layout;
    const float margin = marginDp * dp;
    layout.panel = Rect{room.x + margin, room.y, std::max(room.width - 2.0f * margin, 0.0f),
                        std::max(room.height, 0.0f)};
    layout.radius = radiusDp * dp;

    const float pad = panelPaddingDp * dp;
    const float rowHeight = rowHeightDp * dp;
    const float rowGap = rowGapDp * dp;
    const float visible = std::max(layout.panel.height - 2.0f * pad, 0.0f);
    const float content = static_cast<float>(entries_.size()) * (rowHeight + rowGap) -
                          (entries_.empty() ? 0.0f : rowGap);
    float scroll = 0.0f;
    if (row_ < entries_.size()) {
        const float bottom = static_cast<float>(row_ + 1) * (rowHeight + rowGap) - rowGap;
        scroll = std::max(bottom - visible, 0.0f);
    }
    scroll = std::min(scroll, std::max(content - visible, 0.0f));

    const float inset = insetDp * dp;
    const float buttonWidth = actionWidthDp * dp;
    const float buttonHeight = actionHeightDp * dp;
    const float buttonGap = actionGapDp * dp;
    float y = layout.panel.y + pad - scroll;
    for (const DownloadsEntry& entry : entries_) {
        const Rect row{layout.panel.x + pad, y, layout.panel.width - 2.0f * pad, rowHeight};
        layout.rows.push_back(row);
        std::vector<Rect> buttons;
        float x = row.right() - inset;
        for (std::size_t i = entry.actions.size(); i-- > 0;) {
            x -= buttonWidth;
            buttons.push_back(
                Rect{x, row.centreY() - buttonHeight * 0.5f, buttonWidth, buttonHeight});
            x -= buttonGap;
        }
        std::ranges::reverse(buttons);
        layout.actions.push_back(std::move(buttons));
        const float bodyRight = entry.actions.empty() ? row.right() - inset : x;
        const Rect body{row.x + inset, row.y, std::max(bodyRight - row.x - 2.0f * inset, 0.0f),
                        rowHeight};
        layout.bodies.push_back(body);
        layout.bars.push_back(Rect{body.x, row.bottom() - barBottomDp * dp - barHeightDp * dp,
                                   body.width, barHeightDp * dp});
        y += rowHeight + rowGap;
    }
    return layout;
}

std::optional<std::size_t> DownloadsLayout::rowAt(float x, float y) const noexcept {
    if (!panel.contains(x, y)) {
        return std::nullopt;
    }
    for (std::size_t i = 0; i < rows.size(); ++i) {
        if (rows[i].contains(x, y)) {
            return i;
        }
    }
    return std::nullopt;
}

std::optional<std::pair<std::size_t, std::size_t>>
DownloadsLayout::actionAt(float x, float y) const noexcept {
    const std::optional<std::size_t> row = rowAt(x, y);
    if (!row) {
        return std::nullopt;
    }
    const std::vector<Rect>& buttons = actions[*row];
    for (std::size_t i = 0; i < buttons.size(); ++i) {
        if (buttons[i].contains(x, y)) {
            return std::pair{*row, i};
        }
    }
    return std::nullopt;
}

} // namespace opensu::ui
