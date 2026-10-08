// installs — the installers by store: which stores iideck can install from, and the one install
// that runs at a time.
#pragma once

#include <optional>
#include <string>

#include "epic_install_job.hpp"
#include "install_job.hpp"
#include "library/game.hpp"
#include "steam_install_job.hpp"

namespace iideck::app {

class Installs {
  public:
    /// Installs Epic titles with `legendary`.
    Installs(steam::Client& steam, std::string legendary);

    /// Whether iideck installs from this store. GOG waits for gogdl.
    [[nodiscard]] static bool supports(library::Source source) noexcept;

    /// Starts installing `game` through its store. False while an install runs, or for a store
    /// iideck cannot install from.
    bool start(const library::Game& game);

    /// True from start() until the install's last report has been taken.
    [[nodiscard]] bool running();
    /// The title of the install started last, kept after it ends.
    [[nodiscard]] const std::string& title() const noexcept {
        return title_;
    }

    /// The running install's latest report, once; nothing when there is no news.
    [[nodiscard]] std::optional<InstallJob::Report> take();

    /// The player's answer to the licence agreement the install asked about.
    void decide(bool accepted);

  private:
    /// The job with an install running, or null.
    [[nodiscard]] InstallJob* current();

    SteamInstallJob steam_;
    EpicInstallJob epic_;
    std::string title_;
};

} // namespace iideck::app
