// installs — the installers by store: which stores opensu can install from, and the one install
// that runs at a time.
#pragma once

#include <filesystem>
#include <functional>
#include <optional>
#include <string>

#include "config/config.hpp"
#include "epic_install_job.hpp"
#include "gog_install_job.hpp"
#include "install_job.hpp"
#include "library/game.hpp"
#include "settings/install_folders.hpp"
#include "steam_install_job.hpp"

namespace opensu::app {

class Installs {
  public:
    /// Where a store installs, read when an install starts: nothing leaves the store's own choice.
    using Folders = std::function<std::optional<std::filesystem::path>(library::Source)>;

    /// Installs Epic titles with `legendary` and GOG titles with gogdl, each into the folder
    /// `folders` names for its store.
    Installs(steam::Client& steam, std::string legendary, GogInstallJob::Options gog,
             Folders folders);

    /// Where each store installs when no folder is set: GOG under openSU's data dir (gogdl's
    /// `<data>/gog-games`), Epic where legendary's own default is, Steam its default library
    /// (`steamDefault`, known while Steam runs).
    [[nodiscard]] static settings::StoreFolders
    ownFolders(const config::Config& config, std::optional<std::filesystem::path> steamDefault);

    /// Whether opensu installs from this store.
    [[nodiscard]] static bool supports(library::Source source) noexcept;

    /// Why the folder `game`'s store installs into cannot take an install; empty when it can, or
    /// when the store chooses.
    [[nodiscard]] std::string folderRefusal(library::Source store) const;

    /// Starts installing `game` through its store. False while an install runs, or for a store
    /// opensu cannot install from.
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
    GogInstallJob gog_;
    Folders folders_;
    std::string title_;
};

} // namespace opensu::app
