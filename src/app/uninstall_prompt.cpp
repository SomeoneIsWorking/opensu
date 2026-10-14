#include "uninstall_prompt.hpp"

namespace opensu::app {

std::optional<std::string> UninstallPrompt::arm(const library::Game& game) {
    Removal removal = hooks_.plan(game);
    if (!removal.refusal.empty()) {
        hooks_.say("cannot uninstall " + game.title + ": " + removal.refusal, true);
        return std::nullopt;
    }
    return std::move(removal.what);
}

void UninstallPrompt::confirm(const library::Game& game) {
    if (const std::string refused = hooks_.start(game); !refused.empty()) {
        hooks_.say("cannot uninstall " + game.title + ": " + refused, true);
        return;
    }
    hooks_.say("uninstalling " + game.title, false);
}

} // namespace opensu::app
