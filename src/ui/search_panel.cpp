#include "search_panel.hpp"

#include <algorithm>

namespace opensu::ui {
namespace {

constexpr float cardWidthDp = 600.0f;
constexpr float cardPaddingDp = 12.0f;
constexpr float cardRadiusDp = 17.0f;
constexpr float cardGapDp = 10.0f;
constexpr float fieldHeightDp = 40.0f;
constexpr float sectionGapDp = 8.0f;
constexpr float resultHeightDp = 28.0f;

} // namespace

SearchLayout layoutSearch(const Rect& frame, float dp, input::KeyboardStyle style) {
    SearchLayout layout;
    // The card brings its own field, so the keyboard needs no preview line.
    layout.keyboard = layoutKeyboard(frame, dp, style, false);
    const float pad = cardPaddingDp * dp;
    const float width = std::min(cardWidthDp * dp, frame.width);
    const float height = 2.0f * pad + fieldHeightDp * dp + sectionGapDp * dp +
                         static_cast<float>(searchResultRows) * resultHeightDp * dp;
    const float bottom = layout.keyboard.panel.y - cardGapDp * dp;
    layout.card = Rect{frame.x + (frame.width - width) * 0.5f, std::max(frame.y, bottom - height),
                       width, height};
    layout.cardRadius = cardRadiusDp * dp;
    const float inner = width - 2.0f * pad;
    float y = layout.card.y + pad;
    layout.field = Rect{layout.card.x + pad, y, inner, fieldHeightDp * dp};
    y += (fieldHeightDp + sectionGapDp) * dp;
    for (Rect& row : layout.results) {
        row = Rect{layout.card.x + pad, y, inner, resultHeightDp * dp};
        y += resultHeightDp * dp;
    }
    return layout;
}

Rect SearchLayout::cardFor(std::size_t listed) const noexcept {
    const std::size_t shown = std::min(listed, searchResultRows);
    // With nothing listed the card ends a padding under the field, as it does under the last row.
    const float last = shown == 0 ? field.bottom() : results[shown - 1].bottom();
    return Rect{card.x, card.y, card.width,
                card.height - (results[searchResultRows - 1].bottom() - last)};
}

std::optional<std::size_t> SearchLayout::resultAt(float x, float y) const noexcept {
    for (std::size_t i = 0; i < results.size(); ++i) {
        if (results[i].contains(x, y)) {
            return i;
        }
    }
    return std::nullopt;
}

bool SearchPanel::enterKeys() noexcept {
    const bool changed = zone_ != SearchZone::Keys;
    zone_ = SearchZone::Keys;
    return changed;
}

bool SearchPanel::enterResults() {
    return focusResult(std::min(result_, results_.empty() ? std::size_t{0} : results_.size() - 1));
}

bool SearchPanel::focusResult(std::size_t index) {
    if (index >= results_.size()) {
        return false;
    }
    const bool changed = index != result_ || zone_ != SearchZone::Results;
    result_ = index;
    zone_ = SearchZone::Results;
    keepResultInView();
    return changed;
}

bool SearchPanel::switchZone() {
    if (zone_ == SearchZone::Results) {
        return enterKeys();
    }
    return enterResults();
}

bool SearchPanel::moveInResults(Direction direction) {
    if (direction == Direction::Up && result_ > 0) {
        return focusResult(result_ - 1);
    }
    if (direction == Direction::Down) {
        if (result_ + 1 < results_.size()) {
            return focusResult(result_ + 1);
        }
        return enterKeys();
    }
    return false;
}

void SearchPanel::setResults(std::vector<SearchResult> results) {
    results_ = std::move(results);
    result_ = std::min(result_, results_.empty() ? std::size_t{0} : results_.size() - 1);
    if (results_.empty() && zone_ == SearchZone::Results) {
        zone_ = SearchZone::Keys;
    }
    keepResultInView();
}

void SearchPanel::keepResultInView() noexcept {
    if (result_ < first_) {
        first_ = result_;
    } else if (result_ >= first_ + searchResultRows) {
        first_ = result_ + 1 - searchResultRows;
    }
    first_ =
        std::min(first_, results_.size() > searchResultRows ? results_.size() - searchResultRows
                                                            : std::size_t{0});
}

} // namespace opensu::ui
