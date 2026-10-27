#include "uninstall_prompt.hpp"

namespace opensu::app {

std::optional<std::string> UninstallPrompt::removal(const library::Game& game) {
    Removal plan = hooks_.plan(game);
    if (!plan.refusal.empty()) {
        hooks_.say("cannot uninstall " + game.title + ": " + plan.refusal, true);
        return std::nullopt;
    }
    return std::move(plan.what);
}

void UninstallPrompt::start(const library::Game& game) {
    if (const std::string refused = hooks_.start(game); !refused.empty()) {
        hooks_.say("cannot uninstall " + game.title + ": " + refused, true);
        return;
    }
    hooks_.say("uninstalling " + game.title, false);
}

} // namespace opensu::app
