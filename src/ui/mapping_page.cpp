#include "mapping_page.hpp"

#include <algorithm>

namespace opensu::ui {
namespace {

constexpr float cardWidthDp = 560.0f;
constexpr float paddingDp = 16.0f;
constexpr float titleHeightDp = 34.0f;
constexpr float noteHeightDp = 26.0f;
constexpr float rowHeightDp = 40.0f;
constexpr float rowGapDp = 4.0f;
constexpr float radiusDp = 17.0f;
constexpr float hintBandDp = 30.0f;

} // namespace

MappingLayout layoutMapping(const Rect& frame, float dp, std::size_t count, std::size_t focus) {
    MappingLayout layout;
    const float pad = paddingDp * dp;
    const float width = std::min(cardWidthDp * dp, frame.width);
    const float fixed = pad + titleHeightDp * dp + noteHeightDp * dp + hintBandDp * dp + pad * 0.5f;
    const float step = (rowHeightDp + rowGapDp) * dp;
    // As many rows as the frame has room for, up to `mappingRowsShown`.
    const auto room = static_cast<std::size_t>(std::max((frame.height - fixed) / step, 1.0f));
    const std::size_t shown = std::min({count, mappingRowsShown, room});
    if (shown > 0 && focus >= shown) {
        layout.first = std::min(focus + 1 - shown, count - shown);
    }
    const auto rows = static_cast<float>(shown);
    const float height =
        fixed + rows * rowHeightDp * dp + std::max(rows - 1.0f, 0.0f) * rowGapDp * dp;
    layout.card = Rect{frame.x + (frame.width - width) * 0.5f,
                       frame.y + (frame.height - height) * 0.5f, width, height};
    layout.radius = radiusDp * dp;
    layout.title =
        Rect{layout.card.x + pad, layout.card.y + pad, width - 2.0f * pad, titleHeightDp * dp};
    layout.note =
        Rect{layout.title.x, layout.title.bottom(), layout.title.width, noteHeightDp * dp};
    float y = layout.note.bottom();
    for (std::size_t i = 0; i < shown; ++i) {
        layout.rows.push_back(Rect{layout.card.x + pad, y, width - 2.0f * pad, rowHeightDp * dp});
        y += step;
    }
    layout.hints = Rect{layout.card.x + pad, y, width - 2.0f * pad, hintBandDp * dp};
    return layout;
}

void MappingPage::open(std::string title, std::vector<MappingRow> rows, std::string note) {
    title_ = std::move(title);
    rows_ = std::move(rows);
    note_ = std::move(note);
    focus_ = 0;
    mode_ = MappingMode::Rows;
    choices_.clear();
    open_ = true;
}

void MappingPage::refresh(std::vector<MappingRow> rows, std::string note) {
    const std::string id = focused() != nullptr ? focused()->id : std::string{};
    rows_ = std::move(rows);
    note_ = std::move(note);
    const auto same = std::ranges::find(rows_, id, &MappingRow::id);
    if (same != rows_.end()) {
        focus_ = static_cast<std::size_t>(same - rows_.begin());
    } else if (!rows_.empty()) {
        focus_ = std::min(focus_, rows_.size() - 1);
    } else {
        focus_ = 0;
    }
}

std::string_view MappingPage::state() const noexcept {
    if (!open_) {
        return {};
    }
    switch (mode_) {
    case MappingMode::Rows:
        return "rows";
    case MappingMode::Picking:
        return "picking";
    case MappingMode::Listening:
        return "listening";
    }
    return {};
}

const MappingRow* MappingPage::focused() const {
    return focus_ < rows_.size() ? &rows_[focus_] : nullptr;
}

bool MappingPage::move(int delta) noexcept {
    if (mode_ == MappingMode::Listening) {
        return false;
    }
    std::size_t& at = mode_ == MappingMode::Picking ? choice_ : focus_;
    const std::size_t count = lines();
    if (count == 0) {
        return false;
    }
    const auto target =
        std::clamp(static_cast<long long>(at) + delta, 0LL, static_cast<long long>(count) - 1);
    if (static_cast<std::size_t>(target) == at) {
        return false;
    }
    at = static_cast<std::size_t>(target);
    return true;
}

void MappingPage::pick(std::string title, std::vector<std::string> choices, std::size_t focus) {
    pickTitle_ = std::move(title);
    choices_ = std::move(choices);
    choice_ = choices_.empty() ? 0 : std::min(focus, choices_.size() - 1);
    mode_ = MappingMode::Picking;
}

void MappingPage::listen(std::string note) {
    note_ = std::move(note);
    mode_ = MappingMode::Listening;
}

void MappingPage::showRows(std::string note) {
    note_ = std::move(note);
    mode_ = MappingMode::Rows;
    choices_.clear();
}

} // namespace opensu::ui
