#include "game_menu.hpp"

#include <algorithm>
#include <utility>

namespace opensu::ui {

namespace {

constexpr float panelWidthDp = 300.0f;
constexpr float paddingDp = 24.0f;
constexpr float itemHeightDp = 52.0f;
constexpr float itemGapDp = 6.0f;

} // namespace

std::optional<std::size_t> GameMenuLayout::itemAt(float x, float y) const noexcept {
    for (std::size_t i = 0; i < rows.size(); ++i) {
        if (rows[i].contains(x, y)) {
            return i;
        }
    }
    return std::nullopt;
}

GameMenuLayout layoutGameMenu(float width, float height, float dp, float titleBox) noexcept {
    GameMenuLayout layout;
    const float panel = std::min(panelWidthDp * dp, width);
    layout.panel = Rect{0.0f, 0.0f, panel, height};
    layout.padding = paddingDp * dp;
    const float inset = layout.padding * 0.5f;
    float y = layout.padding + titleBox + layout.padding;
    for (Rect& row : layout.rows) {
        row = Rect{inset, y, panel - 2.0f * inset, itemHeightDp * dp};
        y += itemHeightDp * dp + itemGapDp * dp;
    }
    return layout;
}

void GameMenu::open(std::string title) {
    title_ = std::move(title);
    focus_ = 0;
    open_ = true;
}

void GameMenu::close() noexcept {
    open_ = false;
}

void GameMenu::move(int delta) noexcept {
    const auto last = static_cast<long>(items_.size()) - 1;
    focus_ = static_cast<std::size_t>(std::clamp(static_cast<long>(focus_) + delta, 0L, last));
}

bool GameMenu::focusItem(std::size_t index) noexcept {
    if (index >= items_.size() || index == focus_) {
        return false;
    }
    focus_ = index;
    return true;
}

} // namespace opensu::ui
