#include "pointing_pads.hpp"

namespace opensu::app {

bool PointingPads::take(const gamepad::Event& event) {
    if (event.kind == gamepad::Event::Kind::Pointer) {
        if (event.onScreen) {
            spots_[event.source] = Spot{.x = event.x, .y = event.y};
            hooks_.move(event.x, event.y);
        }
        return true;
    }
    if (event.kind != gamepad::Event::Kind::PointerButton) {
        return false;
    }
    const auto spot = spots_.find(event.source);
    if (spot != spots_.end()) {
        hooks_.press(spot->second.x, spot->second.y, event.pressed);
    }
    return true;
}

} // namespace opensu::app
