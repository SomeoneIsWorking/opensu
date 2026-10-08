#include "game_menu.hpp"

#include <algorithm>
#include <utility>

namespace iideck::ui {

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

} // namespace iideck::ui
