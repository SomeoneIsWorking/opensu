// plugin_worker — one plugin on a thread of its own: its Lua runtime and `opensu` table, its
// callbacks run as coroutines, the I/O they wait on sent to the shared pool and resumed when it
// is done, and its faults counted. A refresh lists the emulator's games (when the plugin has a
// game list), matches them to openSU's, then reads play time; what comes of it is handed to the
// host as plain C++. Three faults in a row stop the plugin; nothing a plugin does stops openSU.
#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <stop_token>
#include <string>
#include <thread>
#include <vector>

#include "host_api.hpp"
#include "io_pool.hpp"
#include "library/emulator_game_list.hpp"
#include "library/game.hpp"
#include "library/play_time.hpp"
#include "lua_runtime.hpp"
#include "manifest.hpp"
#include "net/web_client.hpp"
#include "permissions.hpp"

namespace opensu::plugin {

/// Faults in a row that stop a plugin for the rest of the run.
inline constexpr int faultLimit = 3;

/// The largest `main.lua` loaded.
inline constexpr std::size_t sourceLimit = std::size_t{1} << 20U;

/// What a worker tells the host.
struct WorkerNews {
    enum class Kind : std::uint8_t {
        /// `main.lua` ran and registered its sources.
        Loaded,
        /// A refresh finished.
        Report,
        /// A callback failed; the plugin runs on.
        Fault,
        /// The plugin stopped: it failed to load or reached the fault limit.
        Stopped,
    };
    std::string plugin;
    Kind kind{Kind::Loaded};
    /// For a report: seconds per game id; nothing when the plugin had no new answer.
    std::optional<library::PlayTimeReports::Report> playTime;
    /// For a report with a game list: its entries and how many matched openSU's games.
    std::optional<std::size_t> listed;
    std::size_t matched{0};
    /// For a fault or a stop: why.
    std::string message;
};

/// The news of every worker, collected for the loop.
class Outbox {
  public:
    void post(WorkerNews news);
    [[nodiscard]] std::vector<WorkerNews> take();

  private:
    std::mutex mutex_;
    std::vector<WorkerNews> news_;
};

class PluginWorker {
  public:
    struct Setup {
        Manifest manifest;
        /// Where `main.lua` is.
        std::filesystem::path dir;
        Permissions permissions;
        Limits limits;
    };

    /// Starts the thread, which loads `main.lua`. `pool`, `web` and `outbox` outlive the worker.
    PluginWorker(Setup setup, IoPool& pool, const net::WebClient& web, Outbox& outbox);
    /// Stops a running callback at its next check and joins the thread.
    ~PluginWorker();
    PluginWorker(const PluginWorker&) = delete;
    PluginWorker& operator=(const PluginWorker&) = delete;
    PluginWorker(PluginWorker&&) = delete;
    PluginWorker& operator=(PluginWorker&&) = delete;

    /// Asks for a refresh over `games`; one already running is followed by one more with the
    /// latest games. Any thread.
    void refresh(std::vector<library::Game> games);

    /// Waits until nothing is queued or waiting on I/O; false when `timeout` passed first.
    bool waitIdle(std::chrono::milliseconds timeout);

  private:
    /// The queue the thread runs, shared with I/O jobs that may finish after the worker is gone.
    struct Inbox {
        std::mutex mutex;
        std::condition_variable_any wake;
        std::condition_variable settled;
        std::deque<std::function<void()>> tasks;
        /// Queued tasks plus I/O in flight.
        std::size_t outstanding{0};
        bool closed{false};

        void post(std::function<void()> task);
        /// Counts I/O sent to the pool; `settle` when it has come back.
        void begin();
        void settle();
    };

    /// A callback in flight and what follows it.
    struct Running {
        std::unique_ptr<Activation> activation;
        std::function<void(Activation&)> done;
        std::function<void(const std::string&)> failed;
    };

    void run(const std::stop_token& stop);
    void load();
    void beginRefresh();
    void runPlayTime();
    void endRefresh(const std::string& failure,
                    std::optional<library::PlayTimeReports::Report> report);
    void start(int function, const std::function<int(lua_State*)>& arguments,
               std::function<void(Activation&)> done,
               std::function<void(const std::string&)> failed);
    void step(std::uint64_t id);
    void fault(const std::string& message);
    void halt(const std::string& message);
    [[nodiscard]] std::vector<library::Game> claimed(const Claims& claims) const;
    [[nodiscard]] WorkerNews news(WorkerNews::Kind kind, std::string message) const;

    Setup setup_;
    IoPool& pool_;
    const net::WebClient& web_;
    Outbox& outbox_;
    std::atomic<bool> stopping_{false};
    std::shared_ptr<Inbox> inbox_{std::make_shared<Inbox>()};

    // The thread's own state from here.
    std::unique_ptr<LuaRuntime> runtime_;
    std::unique_ptr<HostContext> context_;
    std::map<std::uint64_t, Running> running_;
    std::uint64_t nextActivation_{0};
    bool loaded_{false};
    bool refreshing_{false};
    bool halted_{false};
    int faults_{0};
    std::optional<std::vector<library::Game>> wanted_;
    std::vector<library::Game> games_;
    std::vector<library::EmulatorGameEntry> listed_;
    std::map<std::string, std::size_t> matches_;
    bool hasList_{false};

    /// Last, so it stops before the state it runs on goes.
    std::jthread thread_;
};

} // namespace opensu::plugin
