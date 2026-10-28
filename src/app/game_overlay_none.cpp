#include "game_overlay.hpp"

namespace opensu::app {

std::unique_ptr<GameOverlay> openGameOverlay(ShellWindow& /*window*/) {
    return nullptr;
}

} // namespace opensu::app
