#include "shell_handoff.hpp"

namespace opensu::retro::shell_handoff {

bool active() noexcept {
    return false;
}

bool openMenu() {
    return false;
}

bool turnScreen(bool) {
    return false;
}

} // namespace opensu::retro::shell_handoff
