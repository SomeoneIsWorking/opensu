// steam_install_job — one Steam install: walks Steam's installer, waits on the player's licence
// answer, then follows the download in Steam's queue.
#pragma once

#include <filesystem>
#include <optional>

#include "install_job.hpp"
#include "steam/client.hpp"

namespace opensu::app {

class SteamInstallJob final : public InstallJob {
  public:
    explicit SteamInstallJob(steam::Client& steam);
    ~SteamInstallJob();

    /// The Steam library the next install goes into; none leaves Steam's default. Call before
    /// start(), while idle.
    void setFolder(std::optional<std::filesystem::path> folder) {
        folder_ = std::move(folder);
    }

  private:
    void run(const std::stop_token& stop, const std::string& appId) override;
    /// Takes the installer to a queued download; false when it ended, after reporting why.
    bool queue(const std::stop_token& stop, const std::string& appId);
    void follow(const std::stop_token& stop, const std::string& appId);

    steam::Client& steam_;
    std::optional<std::filesystem::path> folder_;
};

} // namespace opensu::app
