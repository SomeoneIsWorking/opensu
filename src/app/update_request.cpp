#include "update_request.hpp"

namespace opensu::app {

void UpdateRequest::request(const library::Game& game) {
    if (const std::string refused = hooks_.start(game); !refused.empty()) {
        hooks_.say("cannot update " + game.title + ": " + refused, true);
        return;
    }
    hooks_.say("updating " + game.title, false);
}

} // namespace opensu::app
