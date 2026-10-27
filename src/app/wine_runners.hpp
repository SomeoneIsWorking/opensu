// wine_runners — keeping openSU's own Wine runners current off the main loop: the latest GE-Proton
// and umu-launcher downloaded, checked and unpacked under `<data>/wine` (`ge_proton`, `umu`,
// `runtime_archive`), checked at start and daily while GOG or Epic is in force, on "Check now", and
// at once when a Windows launch finds none. After a check the GE-Proton builds past the newest two
// are removed unless a game runs on one or is set to it, which keeps it with the reason. Also runs
// a Windows program through a runner for Settings' "Test" row. One job at a time, with progress
// and cancel for the Downloads page.
#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <stop_token>
#include <string>
#include <thread>
#include <vector>

#include "library/ge_proton.hpp"
#include "library/runtime_archive.hpp"
#include "library/wine.hpp"
#include "net/web_client.hpp"

namespace opensu::app {

class WineRunners {
  public:
    using Clock = std::chrono::steady_clock;

    /// Why each GE-Proton build may not be removed, by its folder: a game runs on it, games are
    /// pinned to it, the player picked it.
    using InUse = std::map<std::filesystem::path, std::string>;

    struct Hooks {
        /// Tells the player something; `error` marks a failure.
        std::function<void(const std::string& text, bool error)> say;
        /// The installed runners changed: runner lists are read again.
        std::function<void()> changed;
        /// Progress moved: the screens that show it refresh.
        std::function<void()> refresh;
        /// Whether Windows games are in force (GOG or Epic), which the background check waits for.
        std::function<bool()> wanted;
        /// The builds that must stay. Main loop only.
        std::function<InUse()> inUse;
        /// Whether a game runs now, which keeps every umu-launcher install.
        std::function<bool()> gameRunning;
    };

    /// The job running now.
    struct Progress {
        /// `ge-proton` or `umu-launcher`.
        std::string id;
        /// The release: `GE-Proton11-11`, `umu-launcher 1.4.4`.
        std::string name;
        library::ArchiveStage stage{library::ArchiveStage::Downloading};
        std::uint64_t received{0};
        /// 0 until the size is known.
        std::uint64_t total{0};
    };

    /// How the last install ended.
    struct Finished {
        enum class Outcome : std::uint8_t { Installed, Cancelled, Failed };
        std::uint64_t serial{0};
        std::string id;
        std::string name;
        Outcome outcome{Outcome::Installed};
        std::string text;
    };

    /// One installed piece with its size.
    struct Piece {
        std::string name;
        std::filesystem::path dir;
        /// 0 until measured.
        std::uint64_t bytes{0};
    };

    /// What is installed and how the checks went, for Settings › Wine.
    struct Look {
        /// openSU's GE-Proton builds, newest first.
        std::vector<Piece> builds;
        std::optional<Piece> umu;
        /// The Steam Linux Runtime umu fetched.
        std::optional<Piece> steamRuntime;
        /// GE-Proton builds a rotation kept past the newest two, with why.
        std::map<std::string, std::string> kept;
        bool checking{false};
        /// The last check's outcome in words; empty before the first.
        std::string lastCheck;
        /// The last test run's outcome; empty before the first.
        std::string lastTest;
        bool testing{false};
    };

    /// `geApi` is the GE-Proton release JSON (`Config::geProtonRelease`; empty for the latest).
    WineRunners(std::filesystem::path dataDir, std::string geApi, Hooks hooks);
    ~WineRunners();
    WineRunners(const WineRunners&) = delete;
    WineRunners& operator=(const WineRunners&) = delete;

    /// Starts the check when it is due, hands ended jobs to the hooks, rotates after a check and
    /// sweeps what a rotation removed. Main loop only.
    void service(Clock::time_point now);
    /// Checks now; refused, with the reason, while a job runs.
    [[nodiscard]] std::string checkNow();
    /// A launch needs a GE-Proton build and umu-launcher: checks now unless a job runs already.
    void ensure();
    /// Stops the running download; what is installed stays.
    void cancel();
    /// Runs `command` (`env NAME=value ... program args`) as the test of a runner, its output
    /// logged; refused, with the reason, while a job runs.
    [[nodiscard]] std::string test(std::string runner, std::vector<std::string> command);

    [[nodiscard]] bool busy() const;
    [[nodiscard]] std::optional<Progress> progress() const;
    [[nodiscard]] std::optional<Finished> lastFinished() const;
    [[nodiscard]] Look look() const;
    /// The newest `umu-run`, or empty.
    [[nodiscard]] std::filesystem::path umuRun() const;
    [[nodiscard]] const std::filesystem::path& dataDir() const noexcept {
        return dataDir_;
    }

  private:
    using Job = std::function<void(const std::stop_token&)>;

    /// Starts `job` on the job thread; false while one runs.
    bool start(Job job);
    void runCheck(const std::stop_token& stop);
    /// Installs the latest GE-Proton unless it is installed; false when it failed.
    bool updateGeProton(const std::stop_token& stop);
    /// Installs the latest umu-launcher unless it is installed; false when it failed.
    bool updateUmu(const std::stop_token& stop);
    /// Installs `archive` into `into` as the job `id` named `name`.
    bool install(const std::stop_token& stop, const std::string& id, const std::string& name,
                 const library::RuntimeArchive& archive, const std::filesystem::path& into);
    void runTest(const std::stop_token& stop, const std::string& runner,
                 const std::vector<std::string>& command);
    /// Measures what is installed into `look_`.
    void measure();
    void ended(Finished::Outcome outcome, const std::string& id, const std::string& name,
               std::string text);
    /// Removes the builds past the newest two that nothing uses, and old umu-launcher installs.
    /// Main loop only.
    void rotate();
    void sweep();

    std::filesystem::path dataDir_;
    std::string geApi_;
    Hooks hooks_;
    net::WebClient web_;

    mutable std::mutex mutex_;
    std::optional<Progress> progress_;
    std::optional<Finished> finished_;
    std::vector<Finished> news_;
    Look look_;
    bool running_{false};
    bool checked_{false};
    std::uint64_t serial_{0};
    std::optional<int> shownPercent_;
    std::optional<Clock::time_point> nextCheck_;
    /// Last, so they stop before what they use goes.
    std::jthread job_;
    std::jthread sweeper_;
};

} // namespace opensu::app
