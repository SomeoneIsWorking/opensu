#include "forward_map.hpp"

namespace opensu::gamepad {

std::set<Button> ForwardMap::outputOf(Button button) const {
    const auto found = buttons.find(button);
    return found == buttons.end() ? std::set<Button>{button} : found->second;
}

bool ForwardMap::sends(Button button, Button out) const {
    const auto found = buttons.find(button);
    return found == buttons.end() ? button == out : found->second.contains(out);
}

} // namespace opensu::gamepad
