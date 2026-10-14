#include "game_screen.hpp"

#include <stdexcept>

namespace opensu::app {

void GameScreen::attachOverlay() {
    if (window_ == nullptr) {
        throw std::logic_error{"the overlay was attached before the window"};
    }
    overlay_ = std::make_unique<session::GamescopeOverlay>(window_->x11Window());
    keys_ = std::make_unique<session::GameKeys>();
}

void GameScreen::showWindow(bool shown) {
    if (!hidden_ && window_ != nullptr) {
        window_->setShown(shown);
    }
}

void GameScreen::setRunning(bool running) {
    shown_ = false;
    if (overlay_) {
        if (running) {
            overlay_->enter();
        } else {
            overlay_->leave();
        }
    } else {
        showWindow(!running);
    }
}

bool GameScreen::setShown(bool wanted) {
    if (wanted == shown_) {
        return false;
    }
    shown_ = wanted;
    if (overlay_) {
        overlay_->setShown(wanted);
    } else {
        showWindow(wanted);
    }
    return true;
}

} // namespace opensu::app
