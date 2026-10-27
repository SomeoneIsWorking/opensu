// panel_flow — the card a launch or an install shows while it is on its way, and the questions
// before an install: which store to install from, and a licence to accept. Owns the one install,
// update or uninstall that runs at a time, and tells how an update or uninstall ended. B on the
// card cancels a launch or hides an install, which goes on without it.
#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "gpu2d/ui/navigator.hpp"

#include "audio/sdl_sound_player.hpp"
#include "installs.hpp"
#include "ui/progress_screen.hpp"

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
        /// Tells the player something; `error` marks a refusal.
        std::function<void(const std::string& text, bool error)> say;
        /// Whether a game is running, over which the licence question waits.
        std::function<bool()> gameRunning;
    };

    /// The card and the questions go on `navigator`. Installs go through `steam` for Steam
    /// titles, `legendary` for Epic's and gogdl for GOG's.
    PanelFlow(gpu2d::ui::Navigator& navigator, audio::SdlSoundPlayer& sounds, steam::Client& steam,
              const std::filesystem::path& dataDir, Hooks hooks);

    /// Whether the card is up for a launch or an install.
    [[nodiscard]] bool active() const;

    /// Shows the card for a launch of `title` on its way.
    void showLaunch(const std::string& title);
    /// The launch's stage, and how far through it when it has a measure.
    void update(std::string line, std::optional<double> fraction);
    /// Takes the card down if it was up for a launch.
    void endLaunch();

    /// Asks whether to install `game`, from the stores that own it among `copies`.
    void offerInstall(const library::Game& game, std::vector<library::Game> copies);

    /// Shows the install job's news on the card and reloads the catalog when it, an update or an
    /// uninstall finishes.
    void service();

    /// The installs and uninstalls, which the Downloads page lists and cancels.
    [[nodiscard]] Installs& installs() noexcept {
        return install_;
    }

  private:
    /// Cancelling: the player cancelled the launch, which goes on until it is over.
    enum class Use : std::uint8_t { None, Launch, Cancelling, Install, Eula };

    /// Whether the card is up for a launch, cancelled or not.
    [[nodiscard]] bool forLaunch() const noexcept {
        return use_ == Use::Launch || use_ == Use::Cancelling;
    }
    /// Whether the licence question waits: a launch holds the card or a game runs.
    [[nodiscard]] bool licenceWaits() const;
    /// The card on the stack, or null.
    [[nodiscard]] const gpu2d::ui::Screen* card() const;
    /// Puts the card up for `title`, at `line`, unless it is up.
    void showCard(const std::string& title, std::string line);
    /// Takes the card down.
    void closeCard();
    /// B on the card: cancels a launch or hides an install.
    void leave();
    void startInstall(const library::Game& game);
    void presentEula();
    /// Tells how an update or uninstall ended; a done one reloads the catalog.
    void workEnded(const InstallJob::Report& report, Installs::Work work);

    gpu2d::ui::Navigator& navigator_;
    audio::SdlSoundPlayer& sounds_;
    Installs install_;
    Hooks hooks_;
    Use use_{Use::None};
    /// What the card shows.
    ui::ProgressCard shown_;
    /// A licence question that arrived while it had to wait.
    bool eulaWaiting_{false};
};

} // namespace opensu::app
