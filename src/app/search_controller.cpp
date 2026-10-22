#include "search_controller.hpp"

#include <vector>

#include "keyboard_pad.hpp"
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
    preferences_.values().view.search = panel_.keyboard().text();
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

bool SearchController::actInResults(gamepad::Button button) {
    switch (button) {
    case gamepad::Button::Up:
    case gamepad::Button::Down:
        moved(panel_.moveInResults(directionOf(button)));
        return true;
    case gamepad::Button::Left:
    case gamepad::Button::Right:
        return true;
    case gamepad::Button::A:
        if (!panel_.results().empty()) {
            openResult(panel_.resultFocus());
        }
        return true;
    case gamepad::Button::X:
        moved(panel_.enterKeys());
        return true;
    default:
        return false;
    }
}

void SearchController::act(gamepad::Button button) {
    ui::SearchPanel& panel = panel_;
    const ui::OnScreenKeyboard& keyboard = panel.keyboard();
    if (panel.zone() == ui::SearchZone::Results) {
        if (actInResults(button)) {
            return;
        }
    } else if (keyboard.style() == input::KeyboardStyle::Regular) {
        // The grid's X is iiSU's "navigate", and Up from its top row enters the results.
        if (button == gamepad::Button::X ||
            (button == gamepad::Button::Up && keyboard.onTopRow() && !panel.results().empty())) {
            moved(panel.switchZone());
            return;
        }
    } else if (button == gamepad::Button::Down) {
        // The wheel's D-pad moves the cursor across; Down enters the results.
        moved(panel.enterResults());
        return;
    }
    switch (actOnKeyboard(panel.keyboard(), button, sounds_)) {
    case KeyboardOutcome::Edited:
        edited();
        break;
    case KeyboardOutcome::Confirm:
    case KeyboardOutcome::Leave:
        close();
        break;
    case KeyboardOutcome::Handled:
        break;
    }
}

void SearchController::typeText(std::string_view text) {
    if (panel_.keyboard().type(text)) {
        edited();
    }
}

void SearchController::backspace() {
    if (panel_.keyboard().backspace()) {
        edited();
    }
}

void SearchController::confirm() {
    if (panel_.zone() == ui::SearchZone::Results && !panel_.results().empty()) {
        openResult(panel_.resultFocus());
        return;
    }
    close();
}

void SearchController::dismiss() {
    close();
}

void SearchController::walk(ui::Direction direction) {
    ui::SearchPanel& panel = panel_;
    // The physical keyboard has no drawn keys to walk, so Down enters the results and Up leaves
    // them.
    if (panel.zone() == ui::SearchZone::Keys) {
        if (direction == ui::Direction::Down) {
            moved(panel.enterResults());
        }
        return;
    }
    if (direction == ui::Direction::Up && panel.resultFocus() == 0) {
        moved(panel.enterKeys());
        return;
    }
    moved(panel.moveInResults(direction));
}

bool SearchController::clear() {
    if (!searching()) {
        return false;
    }
    panel_.keyboard().clear();
    preferences_.values().view.search.clear();
    panel_.setResults({});
    hooks_.reshow(0);
    return true;
}

} // namespace opensu::app
