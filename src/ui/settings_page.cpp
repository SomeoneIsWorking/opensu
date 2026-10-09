#include "settings_page.hpp"

#include <algorithm>
#include <cmath>

namespace opensu::ui {
namespace {

constexpr float marginDp = 40.0f;
constexpr float categoriesWidthDp = 200.0f;
constexpr float columnGapDp = 22.0f;
constexpr float cellHeightDp = 44.0f;
constexpr float cellGapDp = 6.0f;
constexpr float minCellShare = 0.5f;
constexpr float panelPaddingDp = 12.0f;
constexpr float rowHeightDp = 54.0f;
constexpr float rowGapDp = 4.0f;
constexpr float radiusDp = 16.0f;
constexpr float cellRadiusDp = 12.0f;
constexpr float switchWidthDp = 40.0f;
constexpr float switchHeightDp = 22.0f;
constexpr float sliderWidthDp = 170.0f;
constexpr float sliderHeightDp = 22.0f;
constexpr float controlInsetDp = 16.0f;
constexpr float sliderInsetDp = 52.0f;

} // namespace

SettingsLayout layoutSettings(float width, float height, float dp, float topInset,
                              float bottomInset, std::size_t categories,
                              const std::vector<SettingsRow>& rows, std::size_t focused) {
    SettingsLayout layout;
    const float margin = marginDp * dp;
    const float room = std::max(height - topInset - bottomInset, 0.0f);
    const float listWidth = std::min(categoriesWidthDp * dp, width * 0.3f);
    layout.categories = Rect{margin, topInset, listWidth, room};
    layout.radius = radiusDp * dp;
    layout.cellRadius = cellRadiusDp * dp;
    // The cells shrink together when the list would run past the page, down to half their size.
    const float step = (cellHeightDp + cellGapDp) * dp;
    const float wanted = static_cast<float>(categories) * step;
    const float squeeze = wanted > room ? std::max(room / wanted, minCellShare) : 1.0f;
    float y = topInset;
    for (std::size_t i = 0; i < categories; ++i) {
        layout.categoryCells.push_back(Rect{margin, y, listWidth, cellHeightDp * dp * squeeze});
        y += step * squeeze;
    }
    const float left = margin + listWidth + columnGapDp * dp;
    layout.panel = Rect{left, topInset, std::max(width - left - margin, 0.0f), room};

    const float pad = panelPaddingDp * dp;
    const float rowHeight = rowHeightDp * dp;
    const float rowGap = rowGapDp * dp;
    const float contentHeight =
        static_cast<float>(rows.size()) * (rowHeight + rowGap) - (rows.empty() ? 0.0f : rowGap);
    const float visible = std::max(room - 2.0f * pad, 0.0f);
    float scroll = 0.0f;
    if (focused < rows.size()) {
        const float bottom = static_cast<float>(focused + 1) * (rowHeight + rowGap) - rowGap;
        if (bottom > visible) {
            scroll = bottom - visible;
        }
    }
    scroll = std::min(scroll, std::max(contentHeight - visible, 0.0f));
    float rowY = topInset + pad - scroll;
    for (const SettingsRow& row : rows) {
        SettingsRowBox box;
        box.rect = Rect{layout.panel.x + pad, rowY, layout.panel.width - 2.0f * pad, rowHeight};
        if (row.kind == RowKind::Toggle) {
            box.control = Rect{box.rect.right() - controlInsetDp * dp - switchWidthDp * dp,
                               box.rect.centreY() - switchHeightDp * dp * 0.5f, switchWidthDp * dp,
                               switchHeightDp * dp};
        } else if (row.kind == RowKind::Slider) {
            // The track leaves the label two fifths of the row at least.
            const float track = std::min(sliderWidthDp * dp, box.rect.width * 0.4f);
            box.control =
                Rect{box.rect.right() - sliderInsetDp * dp - track,
                     box.rect.centreY() - sliderHeightDp * dp * 0.5f, track, sliderHeightDp * dp};
        }
        layout.rows.push_back(box);
        rowY += rowHeight + rowGap;
    }
    return layout;
}

std::optional<std::size_t> SettingsLayout::categoryAt(float x, float y) const noexcept {
    for (std::size_t i = 0; i < categoryCells.size(); ++i) {
        if (categoryCells[i].contains(x, y)) {
            return i;
        }
    }
    return std::nullopt;
}

std::optional<std::size_t> SettingsLayout::rowAt(float x, float y) const noexcept {
    if (!panel.contains(x, y)) {
        return std::nullopt;
    }
    for (std::size_t i = 0; i < rows.size(); ++i) {
        if (rows[i].rect.contains(x, y)) {
            return i;
        }
    }
    return std::nullopt;
}

std::optional<int> SettingsLayout::levelAt(const SettingsRow& row, std::size_t index, float x,
                                           float y) const noexcept {
    if (row.kind != RowKind::Slider || index >= rows.size() || !panel.contains(x, y) ||
        !rows[index].rect.contains(x, y)) {
        return std::nullopt;
    }
    const Rect& track = rows[index].control;
    if (x < track.x || x >= track.right()) {
        return std::nullopt;
    }
    const float along = (x - track.x) / track.width;
    return std::clamp(
        row.low + static_cast<int>(std::lround(along * static_cast<float>(row.high - row.low))),
        row.low, row.high);
}

void SettingsPage::open(std::vector<SettingsCategory> categories) {
    categories_ = std::move(categories);
    category_ = 0;
    row_ = 0;
    zone_ = SettingsZone::Categories;
    open_ = true;
}

void SettingsPage::refresh(std::vector<SettingsCategory> categories) {
    if (!open_ || categories.empty()) {
        return;
    }
    const std::string category = focusedCategory().id;
    const SettingsRow* focused = focusedRow();
    const std::string row = focused != nullptr ? focused->id : std::string{};
    categories_ = std::move(categories);
    const auto same = std::ranges::find(categories_, category, &SettingsCategory::id);
    category_ =
        same == categories_.end() ? 0 : static_cast<std::size_t>(same - categories_.begin());
    const std::vector<SettingsRow>& rows = categories_[category_].rows;
    const auto found = std::ranges::find(rows, row, &SettingsRow::id);
    if (found != rows.end()) {
        row_ = static_cast<std::size_t>(found - rows.begin());
    } else {
        row_ = std::min(row_, rows.empty() ? std::size_t{0} : rows.size() - 1);
    }
    if (zone_ == SettingsZone::Rows && rows.empty()) {
        zone_ = SettingsZone::Categories;
    }
}

const SettingsRow* SettingsPage::focusedRow() const {
    if (zone_ != SettingsZone::Rows || category_ >= categories_.size()) {
        return nullptr;
    }
    const std::vector<SettingsRow>& rows = categories_[category_].rows;
    return row_ < rows.size() ? &rows[row_] : nullptr;
}

bool SettingsPage::move(int delta) noexcept {
    const std::size_t count =
        zone_ == SettingsZone::Categories ? categories_.size() : categories_[category_].rows.size();
    std::size_t& at = zone_ == SettingsZone::Categories ? category_ : row_;
    const int last = static_cast<int>(count) - 1;
    const auto next =
        static_cast<std::size_t>(std::clamp(static_cast<int>(at) + delta, 0, std::max(last, 0)));
    const bool moved = next != at;
    at = next;
    if (moved && zone_ == SettingsZone::Categories) {
        row_ = 0;
    }
    return moved;
}

bool SettingsPage::enterRows() noexcept {
    if (zone_ == SettingsZone::Rows || categories_[category_].rows.empty()) {
        return false;
    }
    zone_ = SettingsZone::Rows;
    row_ = 0;
    return true;
}

bool SettingsPage::leaveRows() noexcept {
    if (zone_ == SettingsZone::Categories) {
        return false;
    }
    zone_ = SettingsZone::Categories;
    return true;
}

bool SettingsPage::focusCategory(std::size_t index) noexcept {
    if (index >= categories_.size()) {
        return false;
    }
    const bool changed = index != category_ || zone_ != SettingsZone::Categories;
    if (index != category_) {
        row_ = 0;
    }
    category_ = index;
    zone_ = SettingsZone::Categories;
    return changed;
}

bool SettingsPage::focusRow(std::size_t index) noexcept {
    if (index >= categories_[category_].rows.size()) {
        return false;
    }
    const bool changed = index != row_ || zone_ != SettingsZone::Rows;
    row_ = index;
    zone_ = SettingsZone::Rows;
    return changed;
}

SettingsLayout SettingsPage::layout(float width, float height, float dp, float topInset,
                                    float bottomInset) const {
    return layoutSettings(width, height, dp, topInset, bottomInset, categories_.size(),
                          categories_[category_].rows, row_);
}

} // namespace opensu::ui
