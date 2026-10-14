// uninstall_job — one store game uninstalled, off the main loop, as an `InstallJob` so it shares
// the install's thread, reports, state and one-at-a-time rule. Steam removes its app through its
// own uninstaller and the job waits for the manifest to go; Epic runs `legendary uninstall <app>
// -y`; GOG deletes the recorded folder only when it lies inside the root openSU gave gogdl, then
// forgets the record. No Wine prefix and no ROM is ever deleted.
#pragma once

#include <filesystem>
#include <optional>
#include <string>

#include "install_job.hpp"
#include "library/game.hpp"
#include "library/gog_installs.hpp"
#include "steam/client.hpp"

namespace opensu::app {

/// What uninstalling a game removes, in words for the confirm, or why it cannot be uninstalled.
struct Removal {
    /// Why not; empty when it can be.
    std::string refusal{};
    /// "Steam uninstalls the Steam copy", "deletes the GOG copy at <folder>", ...
    std::string what{};
};

class UninstallJob final : public InstallJob {
  public:
    /// Epic games go through `legendary`; GOG's records and games are under `dataDir`.
    UninstallJob(steam::Client& steam, std::string legendary, const std::filesystem::path& dataDir);
    ~UninstallJob();

    /// The next start()'s store and the folder that store installs under (GOG's root is inside
    /// it; none is openSU's data directory). Call while idle.
    void setStore(library::Source store, std::optional<std::filesystem::path> folder) {
        store_ = store;
        folder_ = std::move(folder);
    }

    /// What uninstalling `game` would remove, with its store's install `folder` as setStore takes
    /// it; or why it cannot.
    [[nodiscard]] Removal removal(const library::Game& game,
                                  const std::optional<std::filesystem::path>& folder) const;

  private:
    void run(const std::stop_token& stop, const std::string& appId) override;
    void uninstallSteam(const std::stop_token& stop, const std::string& appId);
    void uninstallEpic(const std::stop_token& stop, const std::string& appName);
    void uninstallGog(const std::string& gameId);

    steam::Client& steam_;
    std::string legendary_;
    library::gog::Paths paths_;
    library::gog::InstallRecords records_;
    /// The store and install folder of the job being started. Set while idle.
    library::Source store_{library::Source::Steam};
    std::optional<std::filesystem::path> folder_;
};

} // namespace opensu::app
