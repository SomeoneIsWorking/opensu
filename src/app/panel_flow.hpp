// panel_flow — what the launch panel is up for and what the buttons do while it is: a launch on its
// way, an offer to install, an install under way, or a licence question. Owns the one install that
// runs at a time.
#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "audio/sound_player.hpp"
#include "gamepad/event.hpp"
#include "installs.hpp"
#include "ui/shell.hpp"

namespace opensu::app {

class PanelFlow {
  public:
    /// What the flow calls back into the shell app for.
    struct Hooks {
        /// Abandons a launch whose game has not appeared yet.
        std::function<void()> cancelLaunch;
        /// Reads the stores again, after an install finished.
        std::function<void()> reloadCatalog;
    };

    /// Installs go through `steam` for Steam titles, `legendary` for Epic's and gogdl for GOG's.
    PanelFlow(ui::Shell& shell, audio::SoundPlayer& sounds, steam::Client& steam, Hooks hooks);

    /// Whether the panel is up for anything, which is when it takes every button.
    [[nodiscard]] bool active() const noexcept {
        return use_ != Use::None;
    }

    /// A button while the panel is up.
    void act(gamepad::Button button);

    /// Shows the panel for a launch of `title` on its way.
    void showLaunch(const std::string& title);
    /// Hides the panel if it was up for a launch.
    void endLaunch();

    /// Asks whether to install `game`, from the stores that own it among `copies`.
    void offerInstall(const library::Game& game, std::vector<library::Game> copies);

    /// Shows the install job's news on the panel and reloads the catalog when it finishes.
    void service();

  private:
    enum class Use : std::uint8_t { None, Launch, OfferInstall, Install, Eula };

    /// Shows the panel for `title` with iiSU's Open sound.
    void open(const std::string& title);
    /// Hides the panel the player dismissed, with iiSU's Close sound.
    void dismiss();
    void startInstall(const library::Game& game);
    void presentEula();

    ui::Shell& shell_;
    audio::SoundPlayer& sounds_;
    Installs install_;
    Hooks hooks_;
    Use use_{Use::None};
    /// A licence question that arrived while a launch held the panel.
    bool eulaWaiting_{false};
    /// The copies of a game the panel offers to install, one per store: A takes the first, X the
    /// second.
    std::vector<library::Game> offered_;
};

} // namespace opensu::app
