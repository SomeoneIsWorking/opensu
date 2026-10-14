// update_request — Update on the details page and in the tile menu: one press starts the update
// through `Installs` and tells the player, or tells why it cannot start. An update replaces files
// the store already holds, so it takes no second press.
#pragma once

#include <functional>
#include <string>
#include <utility>

#include "library/game.hpp"

namespace opensu::app {

class UpdateRequest {
  public:
    struct Hooks {
        /// Starts updating `game`; empty when it started, else why not (`Installs::update`).
        std::function<std::string(const library::Game& game)> start;
        /// Tells the player something; `error` marks a refusal.
        std::function<void(const std::string& text, bool error)> say;
    };

    explicit UpdateRequest(Hooks hooks) : hooks_{std::move(hooks)} {
    }

    /// Starts updating `game` and says so, or says why not.
    void request(const library::Game& game);

  private:
    Hooks hooks_;
};

} // namespace opensu::app
