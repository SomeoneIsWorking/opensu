// steam_install_job — one Steam install: walks Steam's installer, waits on the player's licence
// answer, then follows the download in Steam's queue.
#pragma once

#include "install_job.hpp"
#include "steam/client.hpp"

namespace opensu::app {

class SteamInstallJob final : public InstallJob {
  public:
    explicit SteamInstallJob(steam::Client& steam);
    ~SteamInstallJob();

  private:
    void run(const std::stop_token& stop, const std::string& appId) override;
    /// Takes the installer to a queued download; false when it ended, after reporting why.
    bool queue(const std::stop_token& stop, const std::string& appId);
    void follow(const std::stop_token& stop, const std::string& appId);

    steam::Client& steam_;
};

} // namespace opensu::app
