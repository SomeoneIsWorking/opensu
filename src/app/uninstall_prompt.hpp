// uninstall_prompt — the two presses that uninstall a store game, shared by the details page and
// the tile menu: the first asks `Installs` what would go and arms the button with it, or says why
// the game cannot be uninstalled; the second starts the uninstall. Moving focus between them is the
// page's or menu's to undo.
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

    /// The first press: what the second would remove, to arm the button with; nothing once the
    /// player was told why `game` cannot be uninstalled.
    [[nodiscard]] std::optional<std::string> arm(const library::Game& game);
    /// The second press: starts the uninstall and tells the player.
    void confirm(const library::Game& game);

  private:
    Hooks hooks_;
};

} // namespace opensu::app
