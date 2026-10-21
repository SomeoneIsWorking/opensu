// panel_flow — what the launch panel is up for and what the buttons do while it is: a launch on its
// way, an offer to install, an install under way, or a licence question. Owns the one install,
// update or uninstall that runs at a time, and tells how an update or uninstall ended.
#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include "audio/sdl_sound_player.hpp"
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
        /// Reads the stores again, after an install, update or uninstall finished.
        std::function<void()> reloadCatalog;
        /// Where a store installs now, from the settings; nothing leaves the store's choice.
        Installs::Folders installFolder;
        /// Whether a launch of this game runs now, which no uninstall may remove.
        Installs::Playing playing;
    };

    /// Installs go through `steam` for Steam titles, `legendary` for Epic's and gogdl for GOG's.
    PanelFlow(ui::Shell& shell, audio::SdlSoundPlayer& sounds, steam::Client& steam,
              const std::filesystem::path& dataDir, Hooks hooks);

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

    /// Shows the install job's news on the panel and reloads the catalog when it, an update or an
    /// uninstall finishes.
    void service();

    /// The installs and uninstalls, which the Downloads page lists and cancels.
    [[nodiscard]] Installs& installs() noexcept {
        return install_;
    }

  private:
    /// Cancelling: the player cancelled the launch, which goes on until it is over.
    enum class Use : std::uint8_t { None, Launch, Cancelling, OfferInstall, Install, Eula };

    /// Whether the panel is up for a launch, cancelled or not.
    [[nodiscard]] bool forLaunch() const noexcept {
        return use_ == Use::Launch || use_ == Use::Cancelling;
    }

    /// Shows the panel for `title` with iiSU's Open sound.
    void open(const std::string& title);
    /// Hides the panel the player dismissed, with iiSU's Close sound.
    void dismiss();
    void startInstall(const library::Game& game);
    void presentEula();
    /// Tells how an update or uninstall ended; a done one reloads the catalog.
    void workEnded(const InstallJob::Report& report, Installs::Work work);

    ui::Shell& shell_;
    audio::SdlSoundPlayer& sounds_;
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
