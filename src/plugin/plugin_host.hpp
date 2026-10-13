// plugin_host — every plugin openSU knows: the bundled ones (`<assets>/plugins/<id>/`) and the
// player's (`<data dir>/plugins/<id>/`), read from their manifests without running them; a worker
// for each one started; refreshes posted to them; and what they report taken on the loop as plain
// C++. Which plugins run is the caller's choice; the frame loop never runs Lua.
#pragma once

#include <chrono>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "io_pool.hpp"
#include "library/game.hpp"
#include "library/play_time.hpp"
#include "lua_runtime.hpp"
#include "manifest.hpp"
#include "net/web_client.hpp"
#include "permissions.hpp"
#include "plugin_worker.hpp"

namespace opensu::plugin {

/// One plugin as Settings shows it.
struct PluginInfo {
    enum class State : std::uint8_t {
        /// Not running: off, or never turned on.
        Off,
        /// Started; `main.lua` has not finished loading.
        Loading,
        Running,
        /// Stopped for the rest of the run: it failed to load or faulted three times in a row.
        Stopped,
        /// Its manifest was refused; it can never run.
        Refused,
    };

    Manifest manifest;
    std::filesystem::path dir;
    bool bundled{false};
    State state{State::Off};
    /// Why it was refused or stopped, or its last fault.
    std::string problem;
    /// Faults in a row.
    int faults{0};
    /// From the last report: the games it gave play time for, and its game list's size and
    /// matches when it has one.
    std::optional<std::size_t> reported;
    std::optional<std::size_t> listed;
    std::size_t matched{0};
    /// The read paths expanded on this machine, and the hosts.
    std::vector<std::filesystem::path> readPaths;
    std::vector<std::string> hosts;
};

class PluginHost {
  public:
    struct Locations {
        std::filesystem::path bundled;
        std::filesystem::path user;
        PathTokens tokens;
        Limits limits;
    };

    /// What the workers said since the last `take`.
    struct Update {
        /// New play-time answers by plugin id.
        std::map<std::string, library::PlayTimeReports::Report> playTime;
        /// Plugins that stopped, with why.
        std::vector<std::pair<std::string, std::string>> stopped;
    };

    /// Reads every manifest; starts nothing.
    explicit PluginHost(Locations locations);
    ~PluginHost();
    PluginHost(const PluginHost&) = delete;
    PluginHost& operator=(const PluginHost&) = delete;
    PluginHost(PluginHost&&) = delete;
    PluginHost& operator=(PluginHost&&) = delete;

    [[nodiscard]] const std::vector<PluginInfo>& plugins() const noexcept {
        return plugins_;
    }

    /// Starts the plugin with `id` when it is off; it refreshes over the latest games once loaded.
    void start(const std::string& id);
    /// Stops the plugin with `id`, waiting for a callback in progress to reach its next check. It
    /// is off afterwards, so `start` runs it afresh even after the fault limit stopped it.
    void stop(const std::string& id);

    /// Asks every running plugin to refresh over `games`.
    void refresh(const std::vector<library::Game>& games);

    /// What arrived since the last call, if anything did. Loop only.
    [[nodiscard]] std::optional<Update> take();

    /// Waits until no running plugin has work queued or I/O in flight; false on timeout.
    bool waitIdle(std::chrono::milliseconds timeout);

  private:
    void discover(const std::filesystem::path& root, bool bundled);
    PluginInfo* find(const std::string& id);

    Locations locations_;
    std::vector<PluginInfo> plugins_;
    std::vector<library::Game> games_;
    bool haveGames_{false};
    net::WebClient web_;
    Outbox outbox_;
    IoPool pool_;
    /// Last: each joins its thread before the pool and the client go.
    std::map<std::string, std::unique_ptr<PluginWorker>> workers_;
};

} // namespace opensu::plugin
