#include "search_panel.hpp"

#include <algorithm>

namespace opensu::ui {
namespace {

constexpr float resultHeightDp = 28.0f;

} // namespace

SearchLayout layoutSearch(const Rect& frame, float dp, input::KeyboardStyle style) {
    SearchLayout layout;
    layout.keyboard = layoutKeyboard(frame, dp, style,
                                     static_cast<float>(searchResultRows) * resultHeightDp * dp);
    float y = layout.keyboard.reserved.y;
    for (Rect& row : layout.results) {
        row = Rect{layout.keyboard.reserved.x, y, layout.keyboard.reserved.width,
                   resultHeightDp * dp};
        y += resultHeightDp * dp;
    }
    return layout;
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
