#include "game_screen.hpp"

#include "raylib.h"

namespace opensu::app {

void GameScreen::attachOverlay(unsigned long handle) {
    overlay_ = std::make_unique<session::GamescopeOverlay>(handle);
    keys_ = std::make_unique<session::GameKeys>();
}

void GameScreen::setRunning(bool running) {
    shown_ = false;
    // raylib has no ShowWindow or HideWindow: hiding is a window state flag, and showing is
    // clearing it.
    if (overlay_) {
        if (running) {
            overlay_->enter();
        } else {
            overlay_->leave();
        }
    } else if (!hidden_) {
        if (running) {
            SetWindowState(FLAG_WINDOW_HIDDEN);
        } else {
            ClearWindowState(FLAG_WINDOW_HIDDEN);
        }
    }
}

bool GameScreen::setShown(bool wanted) {
    if (wanted == shown_) {
        return false;
    }
    shown_ = wanted;
    if (overlay_) {
        overlay_->setShown(wanted);
    } else if (!hidden_) {
        if (wanted) {
            ClearWindowState(FLAG_WINDOW_HIDDEN);
        } else {
            SetWindowState(FLAG_WINDOW_HIDDEN);
        }
    }
    return true;
}

} // namespace opensu::app
