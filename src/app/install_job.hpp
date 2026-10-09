// install_job — one game downloading, off the main loop: the thread, the latest report the loop
// takes, and the licence answer a store may wait for. A store's installer is a subclass that
// supplies run().
#pragma once

#include <condition_variable>
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
        /// The job is over: installed, or `failure` says why not.
        bool finished{false};
        std::string failure;
    };

    InstallJob(const InstallJob&) = delete;
    InstallJob& operator=(const InstallJob&) = delete;

    /// Starts installing the store's `appId`, called `title`. False while another install runs.
    bool start(std::string appId, std::string title);

    /// True from start() until the job's last report has been taken.
    [[nodiscard]] bool running() const;
    [[nodiscard]] std::string title() const;

    /// The latest report, once; nothing when there is no news.
    [[nodiscard]] std::optional<Report> take();

    /// The player's answer to the licence agreement a report asked about.
    void decide(bool accepted);

  protected:
    InstallJob() = default;
    ~InstallJob() = default;

    /// Stops and joins the thread. A subclass calls this first in its destructor, so the thread
    /// is gone before the subclass's members are.
    void halt();

    /// The install, on the job's thread; it ends by posting a finished report unless `stop` was
    /// requested.
    virtual void run(const std::stop_token& stop, const std::string& appId) = 0;

    void post(Report report);
    /// Waits for decide(); nothing when the job is stopped first.
    std::optional<bool> awaitDecision(const std::stop_token& stop);

  private:
    mutable std::mutex mutex_;
    std::condition_variable_any decided_;
    std::optional<bool> decision_;
    std::optional<Report> pending_;
    std::string title_;
    bool running_{false};
    /// Last, so it is stopped and joined before the rest goes.
    std::jthread thread_;
};

} // namespace opensu::app
