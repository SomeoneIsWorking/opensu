// uninstall_prompt — uninstalling a store game, shared by the details page and the tile menu:
// first `Installs` says what would go, which the question before it names, or why the game
// cannot be uninstalled; once the player confirms, the uninstall starts.
#pragma once

#include <functional>
#include <optional>
#include <string>

#include "library/game.hpp"
#include "uninstall_job.hpp"

namespace opensu::app {

class UninstallPrompt {
  public:
    struct Hooks {
        /// What uninstalling `game` would remove, or why it cannot be (`Installs::removal`).
        std::function<Removal(const library::Game& game)> plan;
        /// Starts uninstalling `game`; empty when it started, else why not (`Installs::uninstall`).
        std::function<std::string(const library::Game& game)> start;
        /// Tells the player something; `error` marks a refusal.
        std::function<void(const std::string& text, bool error)> say;
    };

    explicit UninstallPrompt(Hooks hooks) : hooks_{std::move(hooks)} {
    }

    /// What uninstalling `game` would remove, for the question; nothing once the player was told
    /// why it cannot be uninstalled.
    [[nodiscard]] std::optional<std::string> removal(const library::Game& game);
    /// Starts the uninstall the player confirmed and tells the player.
    void start(const library::Game& game);

  private:
    Hooks hooks_;
};

} // namespace opensu::app
