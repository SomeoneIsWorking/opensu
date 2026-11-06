#include "screen_turn.hpp"

#include "control_server.hpp"
#include "lucent/log.h"
#include "presenter.hpp"
#include "shell_handoff.hpp"

namespace opensu::retro {

ScreenTurn::ScreenTurn(Presenter& presenter, ControlServer* control, bool turned)
    : presenter_{presenter}, control_{control}, turned_{turned} {
    // Nothing turns unasked: a game with no choice stays as the Activity started.
    if (turned_) {
        apply();
    }
}

void ScreenTurn::set(bool turned) {
    if (turned == turned_) {
        return;
    }
    turned_ = turned;
    apply();
    if (control_ != nullptr) {
        control_->noteTurned(turned_);
    }
}

void ScreenTurn::apply() const {
    lucent::info("retro", "Rotate screen is {}", turned_ ? "on" : "off");
    if (shell_handoff::active()) {
        shell_handoff::turnScreen(turned_);
    } else {
        presenter_.turnPicture(turned_);
    }
}

} // namespace opensu::retro
