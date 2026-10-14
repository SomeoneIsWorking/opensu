// installs — the installers by store and the uninstaller: which stores opensu can install from,
// and the one install or uninstall that runs at a time.
#pragma once

#include <cstdint>
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
#include "uninstall_job.hpp"

namespace opensu::app {

class Installs {
  public:
    /// Where a store installs, read when an install starts: nothing leaves the store's own choice.
    using Folders = std::function<std::optional<std::filesystem::path>(library::Source)>;
    /// Whether a launch of this game runs now.
    using Playing = std::function<bool(const library::Game&)>;

    /// Installs and uninstalls Epic titles with `legendary` and installs GOG titles with gogdl,
    /// each into the folder `folders` names for its store. `playing` says which game runs.
    Installs(steam::Client& steam, const std::string& legendary, const GogInstallJob::Options& gog,
             Folders folders, Playing playing);

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

    /// Starts installing `game` through its store. False while an install or uninstall runs, or for
    /// a store opensu cannot install from.
    bool start(const library::Game& game);

    /// What uninstalling `game` would remove, or why it cannot be now: a ROM, a game that is not
    /// installed, runs, installs, or waits for another job to end, or the store's own refusal.
    [[nodiscard]] Removal removal(const library::Game& game);
    /// Starts uninstalling `game` from its store; empty when it started, else why not.
    [[nodiscard]] std::string uninstall(const library::Game& game);

    /// "<title> is still installing" (or uninstalling) while a job runs; empty when none does.
    [[nodiscard]] std::string busy();

    /// True from start() or uninstall() until the job's last report has been taken.
    [[nodiscard]] bool running();
    /// The title of the job started last, kept after it ends.
    [[nodiscard]] const std::string& title() const noexcept {
        return title_;
    }

    /// The running job's latest report, once; nothing when there is no news.
    [[nodiscard]] std::optional<InstallJob::Report> take();

    /// The player's answer to the licence agreement the install asked about.
    void decide(bool accepted);

    enum class Work : std::uint8_t { Install, Uninstall };

    /// The job started last and where it stands, kept after it ends; nothing before the first.
    struct Status {
        library::Source store{library::Source::Steam};
        Work work{Work::Install};
        InstallJob::Status job;
    };
    [[nodiscard]] std::optional<Status> status() const;

    /// Stops the running job; it ends as cancelled.
    void cancel();

  private:
    /// The job running, or null.
    [[nodiscard]] InstallJob* current();

    SteamInstallJob steam_;
    EpicInstallJob epic_;
    GogInstallJob gog_;
    UninstallJob uninstall_;
    Folders folders_;
    Playing playing_;
    std::string title_;
    /// The job started last and its store.
    InstallJob* last_{nullptr};
    library::Source lastStore_{library::Source::Steam};
};

} // namespace opensu::app
