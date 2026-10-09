#include "search_controller.hpp"

#include <vector>

#include "library/library_query.hpp"

namespace opensu::app {

void SearchController::open() {
    // input-sound.md 3.4 Open: a panel appears.
    sounds_.play(audio::Effect::Open);
    panel_.open(preferences_.values().view.search);
    edited();
}

void SearchController::close() {
    // input-sound.md 3.4 Close: the player dismisses a panel.
    sounds_.play(audio::Effect::Close);
    panel_.close();
}

void SearchController::edited() {
    preferences_.values().view.search = panel_.text();
    hooks_.reshow(0);
    panel_.setResults(searching() ? hooks_.results() : std::vector<ui::SearchResult>{});
}

void SearchController::moved(bool changed) {
    // input-sound.md 3.4 Navigation: Up and Down in the Global Search list.
    if (changed) {
        sounds_.play(audio::Effect::Navigation);
    }
}

void SearchController::openResult(std::size_t index) {
    close();
    hooks_.openTile(index);
}

void SearchController::applyPress(const ui::SearchPress& press) {
    if (press.edited) {
        edited();
    }
    if (press.open) {
        openResult(*press.open);
    } else if (press.close) {
        close();
    }
}

void SearchController::act(gamepad::Button button) {
    ui::SearchPanel& panel = panel_;
    switch (button) {
    case gamepad::Button::Up:
        moved(panel.move(ui::Direction::Up));
        break;
    case gamepad::Button::Down:
        moved(panel.move(ui::Direction::Down));
        break;
    case gamepad::Button::Left:
        moved(panel.move(ui::Direction::Left));
        break;
    case gamepad::Button::Right:
        moved(panel.move(ui::Direction::Right));
        break;
    case gamepad::Button::A:
        applyPress(panel.press());
        break;
    case gamepad::Button::B:
        // The text is deleted a character at a time; with none left B closes, as iiSU's B does.
        if (panel.text().empty()) {
            close();
        } else if (panel.backspace()) {
            edited();
        }
        break;
    case gamepad::Button::X:
        moved(panel.switchZone());
        break;
    case gamepad::Button::Y:
        if (!panel.text().empty() && panel.text().back() != ' ' && panel.type(" ")) {
            edited();
        }
        break;
    case gamepad::Button::Select:
        if (panel.clear()) {
            edited();
        }
        break;
    case gamepad::Button::Start:
        close();
        break;
    default:
        break;
    }
}

void SearchController::typeText(std::string_view text) {
    if (panel_.type(text)) {
        edited();
    }
}

void SearchController::backspace() {
    if (panel_.backspace()) {
        edited();
    }
}

void SearchController::confirm() {
    ui::SearchPanel& panel = panel_;
    if (panel.zone() == ui::SearchZone::Results) {
        applyPress(panel.press());
        return;
    }
    close();
}

void SearchController::dismiss() {
    close();
}

void SearchController::walk(ui::Direction direction) {
    ui::SearchPanel& panel = panel_;
    // The keyboard has no drawn keys to walk, so Down enters the results and Up leaves them.
    if (panel.zone() == ui::SearchZone::Keys) {
        if (direction == ui::Direction::Down && !panel.results().empty()) {
            moved(panel.focusResult(panel.resultFocus()));
        }
        return;
    }
    if (direction == ui::Direction::Up && panel.resultFocus() == 0) {
        moved(panel.switchZone());
        return;
    }
    moved(panel.move(direction));
}

bool SearchController::clear() {
    if (!searching()) {
        return false;
    }
    panel_.clear();
    preferences_.values().view.search.clear();
    panel_.setResults({});
    hooks_.reshow(0);
    return true;
}

} // namespace opensu::app
