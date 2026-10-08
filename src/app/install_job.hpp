// install_job — one Steam game downloading: asks the client to install it, then follows its
// progress until it is installed, off the main loop.
#pragma once

#include <mutex>
#include <optional>
#include <string>
#include <thread>

#include "launch/steam_gate.hpp"
#include "steam/client.hpp"

namespace iideck::app {

class InstallJob {
  public:
    /// What the job has to tell the loop since it last asked.
    struct Report {
        /// The line for the panel ("Installing · 42%").
        std::string line;
        /// 0 to 1 once the download is measured.
        std::optional<double> fraction;
        /// The job is over: installed, or `failure` says why not.
        bool finished{false};
        std::string failure;
    };

    explicit InstallJob(steam::Client& steam);
    InstallJob(const InstallJob&) = delete;
    InstallJob& operator=(const InstallJob&) = delete;

    /// Starts installing `appId`, called `title`. False while another install runs.
    bool start(std::string appId, std::string title);

    /// True from start() until the job's last report has been taken.
    [[nodiscard]] bool running() const;
    [[nodiscard]] std::string title() const;

    /// The latest report, once; nothing when there is no news.
    [[nodiscard]] std::optional<Report> take();

  private:
    void run(const std::stop_token& stop, const std::string& appId);
    void post(Report report);

    steam::Client& steam_;
    mutable std::mutex mutex_;
    std::optional<Report> pending_;
    std::string title_;
    bool running_{false};
    /// Last, so it is stopped and joined before the rest goes.
    std::jthread thread_;
};

} // namespace iideck::app
