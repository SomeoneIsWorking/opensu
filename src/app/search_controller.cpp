#include "search_controller.hpp"

#include <algorithm>
#include <memory>

#include "search_screen.hpp"

namespace opensu::app {

bool SearchController::isOpen() const {
    const std::vector<const gpu2d::ui::Screen*> open = navigator_.screens();
    return std::ranges::any_of(open, [](const gpu2d::ui::Screen* screen) {
        return dynamic_cast<const SearchScreen*>(screen) != nullptr;
    });
}

void SearchController::open() {
    if (isOpen()) {
        return;
    }
    // input-sound.md 3.4 Open: a panel appears.
    sounds_.play(audio::Effect::Open);
    navigator_.push(std::make_unique<SearchScreen>(*this, chrome_));
    refresh();
    keyboard_.open();
}

void SearchController::close() {
    for (const gpu2d::ui::Screen* screen : navigator_.screens()) {
        if (dynamic_cast<const SearchScreen*>(screen) != nullptr) {
            // input-sound.md 3.4 Close: the player dismisses a panel.
            sounds_.play(audio::Effect::Close);
            navigator_.closeFrom(*screen);
            return;
        }
    }
}

void SearchController::refresh() {
    hooks_.reshow(0);
    results_ = searching() ? hooks_.results() : std::vector<SearchResult>{};
}

void SearchController::edit(std::string text) {
    if (text.starts_with(' ') || text.find("  ") != std::string::npos) {
        return;
    }
    preferences_.values().view.search = std::move(text);
    refresh();
}

void SearchController::toResults() {
    if (!results_.empty()) {
        navigator_.focus(SearchScreen::resultId(0));
    }
}

void SearchController::openResult(std::size_t index) {
    close();
    hooks_.openTile(index);
}

bool SearchController::clear() {
    if (!searching()) {
        return false;
    }
    preferences_.values().view.search.clear();
    results_.clear();
    hooks_.reshow(0);
    return true;
}

} // namespace opensu::app
