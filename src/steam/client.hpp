// steam — the Steam client iideck owns.
//
// Steam starts in the background when iideck does, inside a scope of the session,
// so closing iideck closes Steam. Games are then launched through that client, and
// a launch waits until it is logged on.
#pragma once

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "desktop_steam.hpp"
#include "devtools.hpp"
#include "downloads.hpp"
#include "install_wizard.hpp"
#include "launch/instance.hpp"
#include "launch/steam_gate.hpp"
#include "launch_activity.hpp"

namespace iideck::steam {

/// How long shutdown() lets Steam exit on its own before the scope is stopped.
inline constexpr std::chrono::seconds shutdownWait{20};

/// Starts, watches and stops the background Steam client.
class Client final : public launch::SteamGate {
  public:
    struct Options {
        /// Where `.steam/` lives: the client's pid file and its connection log.
        std::filesystem::path home;
        /// Where `steam` is looked up.
        std::vector<std::filesystem::path> executablePath;
        /// Names the scope: `<session>-steam.scope`.
        std::string session;
        /// Install roots holding the app manifests; discovered under `home` when empty.
        std::vector<std::filesystem::path> steamRoots;
    };

    explicit Client(Options options);
    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;
    ~Client() override;

    /// Starts `steam -silent -cef-enable-debugging` in its scope, on a DBus session bus of its own,
    /// and begins watching it. The state is Blocked when a Steam client already runs outside
    /// iideck, and Failed when Steam cannot be started. Does nothing unless the state is Stopped.
    void start();

    /// Asks the client to exit with `steam -shutdown`, waits up to shutdownWait for
    /// it, then stops the scope. The state ends as Stopped.
    void shutdown();

    /// Steam's installer, driven through the running client.
    [[nodiscard]] InstallWizard& installer() noexcept {
        return installer_;
    }
    /// True once the app's manifest says it is fully installed with no update pending.
    [[nodiscard]] bool installed(std::string_view appId) const;
    /// Steam's unfinished downloads as last read, at most a second old while Steam is ready.
    [[nodiscard]] std::vector<Download> downloads() const;

    [[nodiscard]] launch::SteamState state() const override;
    [[nodiscard]] launch::SteamState waitReady(std::chrono::milliseconds timeout,
                                               const std::function<bool()>& cancelled) override;
    [[nodiscard]] std::optional<double> updateProgress(std::string_view appId) const override;
    [[nodiscard]] launch::SteamAppActivity activity(std::string_view appId) const override;

  private:
    /// The watcher thread: polls the scope and the connection log until stopped.
    void watch();

    /// True once a logon completed after start(). Reads only what was appended.
    [[nodiscard]] bool logonCompleted();

    void setState(launch::SteamState state);
    /// Rereads the download queue and launch activity; keeps the last of each when Steam
    /// cannot be asked.
    void refreshFromClient();

    Options options_;
    DesktopSteam desktop_;
    std::filesystem::path program_;
    std::filesystem::path connectionLog_;
    /// How much of the connection log predates this client; only later lines count.
    std::uintmax_t logOffset_{0};
    launch::Instance instance_;
    DevTools devTools_;
    DownloadQueue queue_{devTools_};
    InstallWizard installer_{devTools_};
    LaunchActivity launches_{devTools_};

    mutable std::mutex mutex_;
    std::condition_variable changed_;
    launch::SteamState state_{launch::SteamState::Stopped};
    bool stopping_{false};
    std::vector<Download> downloads_;
    Activities activities_;
    /// The last reason the queue could not be read, logged once per change.
    std::string queueError_;
    std::thread watcher_;
};

} // namespace iideck::steam
