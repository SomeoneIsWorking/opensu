#include "cover_picker.hpp"

#include <algorithm>
#include <cmath>

namespace opensu::ui {
namespace {

constexpr float cardWidthDp = 1040.0f;
constexpr float paddingDp = 24.0f;
constexpr float titleHeightDp = 34.0f;
constexpr float statusHeightDp = 26.0f;
constexpr float cellDp = 176.0f;
constexpr float cellGapDp = 16.0f;
constexpr float labelHeightDp = 28.0f;
constexpr float hintBandDp = 40.0f;
constexpr float radiusDp = 22.0f;
constexpr float frameMarginDp = 24.0f;

} // namespace

std::optional<std::size_t> CoverPickerLayout::pickAt(float x, float y) const noexcept {
    for (std::size_t i = 0; i < cells.size(); ++i) {
        if (cells[i].contains(x, y)) {
            return first + i;
        }
    }
    return std::nullopt;
}

CoverPickerLayout layoutCoverPicker(const Rect& frame, float dp, std::size_t count,
                                    std::size_t first) {
    CoverPickerLayout layout;
    const float pad = paddingDp * dp;
    const float width = std::min(cardWidthDp * dp, frame.width - 2.0f * frameMarginDp * dp);
    const float cell = cellDp * dp;
    const float gap = cellGapDp * dp;
    const float row = width - 2.0f * pad;
    layout.capacity =
        std::max<std::size_t>(1, static_cast<std::size_t>(std::floor((row + gap) / (cell + gap))));
    const float height = pad + titleHeightDp * dp + statusHeightDp * dp + gap + cell +
                         labelHeightDp * dp + hintBandDp * dp + pad * 0.5f;
    layout.card = Rect{frame.x + (frame.width - width) * 0.5f,
                       frame.y + (frame.height - height) * 0.5f, width, height};
    layout.radius = radiusDp * dp;
    layout.title = Rect{layout.card.x + pad, layout.card.y + pad, row, titleHeightDp * dp};
    layout.status = Rect{layout.title.x, layout.title.bottom(), row, statusHeightDp * dp};
    layout.first = std::min(first, count);
    const std::size_t shown = std::min(layout.capacity, count - layout.first);
    const float top = layout.status.bottom() + gap;
    for (std::size_t i = 0; i < shown; ++i) {
        layout.cells.push_back(
            Rect{layout.title.x + static_cast<float>(i) * (cell + gap), top, cell, cell});
    }
    layout.hints = Rect{layout.title.x, top + cell + labelHeightDp * dp, row, hintBandDp * dp};
    return layout;
}

void CoverPicker::open(std::string title, std::vector<CoverPick> picks, std::string status) {
    title_ = std::move(title);
    picks_ = std::move(picks);
    status_ = std::move(status);
    focus_ = 0;
    first_ = 0;
    ++generation_;
    open_ = true;
}

void CoverPicker::update(std::vector<CoverPick> picks, std::string status) {
    picks_ = std::move(picks);
    status_ = std::move(status);
    focus_ = picks_.empty() ? 0 : std::min(focus_, picks_.size() - 1);
    keepFocusShown();
}

bool CoverPicker::move(int delta) noexcept {
    const auto last = static_cast<long>(picks_.size()) - 1;
    const auto next = static_cast<std::size_t>(
        std::clamp(static_cast<long>(focus_) + delta, 0L, std::max(last, 0L)));
    if (next == focus_) {
        return false;
    }
    focus_ = next;
    keepFocusShown();
    return true;
}

bool CoverPicker::focusPick(std::size_t index) noexcept {
    if (index >= picks_.size() || index == focus_) {
        return false;
    }
    focus_ = index;
    keepFocusShown();
    return true;
}

void CoverPicker::setCapacity(std::size_t capacity) noexcept {
    capacity_ = std::max<std::size_t>(capacity, 1);
    keepFocusShown();
}

void CoverPicker::keepFocusShown() noexcept {
    if (focus_ < first_) {
        first_ = focus_;
    } else if (focus_ >= first_ + capacity_) {
        first_ = focus_ + 1 - capacity_;
    }
}

} // namespace opensu::ui
