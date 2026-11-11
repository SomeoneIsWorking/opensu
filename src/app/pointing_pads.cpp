#include "pointing_pads.hpp"

namespace opensu::app {

bool PointingPads::take(const gamepad::Event& event) {
    if (event.kind == gamepad::Event::Kind::Pointer) {
        Aim& aim = aims_[event.source];
        aim.onScreen = event.onScreen;
        if (event.onScreen) {
            aim.x = event.x;
            aim.y = event.y;
            hooks_.move(event.x, event.y);
        }
        return true;
    }
    if (event.kind != gamepad::Event::Kind::Button || event.button != gamepad::Button::A) {
        return false;
    }
    const auto aim = aims_.find(event.source);
    if (event.pressed) {
        if (aim == aims_.end() || !aim->second.onScreen) {
            return false;
        }
        pressing_.insert(event.source);
        hooks_.press(aim->second.x, aim->second.y, true);
        return true;
    }
    if (pressing_.erase(event.source) == 0 || aim == aims_.end()) {
        return false;
    }
    hooks_.press(aim->second.x, aim->second.y, false);
    return true;
}

} // namespace opensu::app
