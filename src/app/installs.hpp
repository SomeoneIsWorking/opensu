// installs — the installers by store, the updaters and the uninstaller: which stores opensu can
// install from, and the one install, update or uninstall that runs at a time. Updates and
// uninstalls share their refusals.
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
#include "library/gog_installs.hpp"
#include "settings/install_folders.hpp"
#include "settings/integrations.hpp"
#include "steam_install_job.hpp"
#include "steam_update_job.hpp"
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

    /// Where each store `integrations` lists installs when no folder is set: GOG under openSU's
    /// data dir (gogdl's `<data>/gog-games`), Epic where legendary's own default is, Steam its
    /// default library (`steamDefault`, known while Steam runs). A store not listed is not read.
    [[nodiscard]] static settings::StoreFolders
    ownFolders(const config::Config& config, std::optional<std::filesystem::path> steamDefault,
               const settings::Integrations& integrations);

    /// Whether opensu installs from this store.
    [[nodiscard]] static bool supports(library::Source source) noexcept;

    /// Why the folder `game`'s store installs into cannot take an install; empty when it can, or
    /// when the store chooses.
    [[nodiscard]] std::string folderRefusal(library::Source store) const;

    /// Starts installing `game` through its store. False while a job runs, or for a store opensu
    /// cannot install from.
    bool start(const library::Game& game);

    /// Starts updating `game` through its store: Epic with `legendary update`, GOG with
    /// `gogdl update` over its recorded install, Steam by queueing the update in Steam. Empty when
    /// it started, else why not: the refusals an uninstall has, no update known, Steam not
    /// running, a GOG game with no record.
    [[nodiscard]] std::string update(const library::Game& game);

    /// What uninstalling `game` would remove, or why it cannot be now: a ROM, a game that is not
    /// installed, runs, installs, or waits for another job to end, or the store's own refusal.
    [[nodiscard]] Removal removal(const library::Game& game);
    /// Starts uninstalling `game` from its store; empty when it started, else why not.
    [[nodiscard]] std::string uninstall(const library::Game& game);

    /// "<title> is still installing" (or updating, uninstalling) while a job runs; empty when none
    /// does.
    [[nodiscard]] std::string busy();

    /// True from a start until the job's last report has been taken.
    [[nodiscard]] bool running();
    /// The title of the job started last, kept after it ends.
    [[nodiscard]] const std::string& title() const noexcept {
        return title_;
    }

    /// The running job's latest report, once; nothing when there is no news.
    [[nodiscard]] std::optional<InstallJob::Report> take();

    /// The player's answer to the licence agreement the install asked about.
    void decide(bool accepted);

    enum class Work : std::uint8_t { Install, Update, Uninstall };

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
    /// Why `work` cannot be done to `game` now, by the rules updates and uninstalls share: not a
    /// store game, not installed, running, or another job under way. Empty when none applies.
    [[nodiscard]] std::string refusal(const library::Game& game, Work work);
    /// Starts `job` on `game` as `work`; false while it runs.
    bool begin(InstallJob& job, const library::Game& game, Work work);

    /// Whether Steam runs and can be asked.
    std::function<bool()> steamReady_;
    SteamInstallJob steam_;
    SteamUpdateJob steamUpdate_;
    EpicInstallJob epic_;
    GogInstallJob gog_;
    UninstallJob uninstall_;
    library::gog::InstallRecords gogRecords_;
    Folders folders_;
    Playing playing_;
    std::string title_;
    /// The job started last, its store and what it does.
    InstallJob* last_{nullptr};
    library::Source lastStore_{library::Source::Steam};
    Work lastWork_{Work::Install};
};

} // namespace opensu::app
