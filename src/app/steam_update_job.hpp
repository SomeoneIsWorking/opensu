// steam_update_job — one Steam update asked for, as an `InstallJob` so it shares the one-at-a-time
// rule and the Downloads page: Steam updates its games itself, so the job only starts the pending
// update as Steam's library Update button does (`steam::DownloadControl::moveToTop`) and ends;
// Steam's queue row then carries it.
#pragma once

#include <functional>
#include <string>
#include <utility>

#include "install_job.hpp"
#include "steam/download_control.hpp"

namespace opensu::app {

class SteamUpdateJob final : public InstallJob {
  public:
    /// `ready` says whether Steam runs and can be asked; `control` asks it.
    SteamUpdateJob(std::function<bool()> ready, steam::DownloadControl& control)
        : ready_{std::move(ready)}, control_{control} {
    }
    ~SteamUpdateJob();

  private:
    void run(const std::stop_token& stop, const std::string& appId) override;

    std::function<bool()> ready_;
    steam::DownloadControl& control_;
};

} // namespace opensu::app
