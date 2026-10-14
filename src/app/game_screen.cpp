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
    mode_ = session::OverlayMode::Hidden;
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

void GameScreen::show(session::OverlayMode wanted) {
    if (wanted == mode_) {
        return;
    }
    const bool wasInteractive = mode_ == session::OverlayMode::Interactive;
    const bool interactive = wanted == session::OverlayMode::Interactive;
    mode_ = wanted;
    if (overlay_) {
        overlay_->show(wanted);
    } else if (interactive != wasInteractive) {
        showWindow(interactive);
    }
    if (interactive != wasInteractive) {
        blockPads_(interactive);
    }
}

} // namespace opensu::app
