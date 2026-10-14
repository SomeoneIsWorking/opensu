// install_job — one store job on a game, off the main loop: the thread, the latest report the loop
// takes, the state anyone may read, the licence answer a store may wait for, and the player's
// cancel. A store's installer, and the uninstaller, are subclasses that supply run().
#pragma once

#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <optional>
#include <stop_token>
#include <string>
#include <thread>

namespace opensu::app {

class InstallJob {
  public:
    /// What the job has to tell the loop since it last asked.
    struct Report {
        /// The line for the panel ("Installing · 42%").
        std::string line;
        /// 0 to 1 once the download is measured.
        std::optional<double> fraction;
        /// A licence agreement waits on the player; answer with decide().
        bool licence{false};
        /// The job is over: done, or `failure` says why not, or the player cancelled it.
        bool finished{false};
        std::string failure;
        bool cancelled{false};
    };

    /// `Done` is the job's success: the game installed, or uninstalled.
    enum class Phase : std::uint8_t { Running, Licence, Done, Failed, Cancelled };

    /// Where the job stands, kept after it ends until the next start.
    struct Status {
        /// Counts starts, so each job is told apart.
        std::uint64_t serial{0};
        std::string appId{};
        std::string title{};
        Phase phase{Phase::Running};
        /// The latest report's line and measure.
        std::string line{};
        std::optional<double> fraction{};
        std::string failure{};
    };

    InstallJob(const InstallJob&) = delete;
    InstallJob& operator=(const InstallJob&) = delete;

    /// Starts the job on the store's `appId`, called `title`. False while it runs.
    bool start(std::string appId, std::string title);

    /// True from start() until the job's last report has been taken.
    [[nodiscard]] bool running() const;
    [[nodiscard]] std::string title() const;

    /// The latest report, once; nothing when there is no news.
    [[nodiscard]] std::optional<Report> take();

    /// The job's state now, without taking a report; nothing before the first start.
    [[nodiscard]] std::optional<Status> status() const;

    /// Stops the running job; it ends with a cancelled report. Nothing while idle.
    void cancel();

    /// The player's answer to the licence agreement a report asked about.
    void decide(bool accepted);

  protected:
    InstallJob() = default;
    ~InstallJob() = default;

    /// Stops and joins the thread. A subclass calls this first in its destructor, so the thread
    /// is gone before the subclass's members are.
    void halt();

    /// The job, on its thread; it ends by posting a finished report unless `stop` was requested.
    virtual void run(const std::stop_token& stop, const std::string& appId) = 0;

    void post(Report report);
    /// Waits for decide(); nothing when the job is stopped first.
    std::optional<bool> awaitDecision(const std::stop_token& stop);

  private:
    mutable std::mutex mutex_;
    std::condition_variable_any decided_;
    std::optional<bool> decision_;
    std::optional<Report> pending_;
    std::optional<Status> status_;
    std::string title_;
    bool running_{false};
    /// The running install has posted its finished report; later ones are dropped.
    bool over_{false};
    /// Last, so it is stopped and joined before the rest goes.
    std::jthread thread_;
};

} // namespace opensu::app
