// steam — the Steam client openSU launches through, in one of three modes.
//
// Managed: openSU's own Steam starts in the background inside a scope of the session, so closing
// openSU closes Steam. Isolated: the same, with the client inside the bwrap sandbox
// (`sandbox.hpp`), which shows it only openSU's Gamescope and the controllers it is given. Normal:
// the player's Steam on the desktop is attached to when it runs, or started on the desktop,
// outside openSU's scope, and never ended by openSU. Games are launched through the client in
// every mode, and a launch waits until it is logged on.
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

#include "big_picture.hpp"
#include "desktop_steam.hpp"
#include "devtools.hpp"
#include "download_control.hpp"
#include "downloads.hpp"
#include "install_wizard.hpp"
#include "launch/instance.hpp"
#include "launch/steam_gate.hpp"
#include "launch_activity.hpp"
#include "libraries.hpp"
#include "orphaned_steam.hpp"
#include "sandbox.hpp"
#include "secret_bus.hpp"
#include "uninstaller.hpp"

namespace opensu::steam {

/// How long shutdown() lets Steam exit on its own before the scope is stopped.
inline constexpr std::chrono::seconds shutdownWait{20};

/// Starts, watches and stops the background Steam client.
class Client final : public launch::SteamGate {
  public:
    enum class Mode : std::uint8_t { Managed, Normal, Isolated };

    struct Options {
        /// Where `.steam/` lives: the client's pid file and its connection log.
        std::filesystem::path home;
        /// Where `steam` is looked up.
        std::vector<std::filesystem::path> executablePath;
        /// Names the scope: `<session>-steam.scope`.
        std::string session;
        /// Install roots holding the app manifests; discovered under `home` when empty.
        std::vector<std::filesystem::path> steamRoots;
        /// False for a hidden run: `start` decides and logs what it would do, and starts and
        /// attaches to nothing.
        bool mayStart{true};
        /// Where the client's DevTools listen.
        std::uint16_t devToolsPort{steam::devToolsPort};
        /// The session Isolated's sandbox lets Steam reach: openSU's Gamescope display, the
        /// runtime dir and the GPU.
        SandboxPlaces sandbox{};
        /// The desktop's session bus, which Isolated relays only the Secret Service of.
        std::string desktopBus{};
        /// Where Isolated's Secret Service proxy listens.
        std::filesystem::path secretBusSocket{};
    };

    explicit Client(Options options);
    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;
    ~Client() override;

    /// Managed: first ends a Steam left in `opensu-<pid>-steam.scope` by an openSU that died, then
    /// starts `steam -silent -cef-enable-debugging` in its scope, on the desktop's session bus,
    /// and begins watching it. The state is Blocked when a Steam client already runs outside
    /// opensu, and Failed when Steam cannot be started.
    /// Isolated: as Managed, with Steam inside bwrap holding the controllers `devices` names and a
    /// session bus that reaches only the desktop's Secret Service (`SecretBus`, in
    /// `<session>-steam-bus.scope`); Failed when bwrap or xdg-dbus-proxy is missing or openSU
    /// has no Gamescope display.
    /// Normal: attaches to the desktop's running Steam (Ready at once), or starts `steam` on the
    /// desktop through `systemd-run --user` and waits for its logon. Never a second Steam.
    /// Does nothing unless the state is Stopped.
    void start(Mode mode, const SandboxDevices& devices = {});

    /// Managed and Isolated: asks the client to exit with `steam -shutdown`, waits up to
    /// shutdownWait for it, then stops the scope. Normal: stops watching; the desktop's Steam keeps
    /// running. The state ends as Stopped.
    void shutdown();

    /// The mode of the last start.
    [[nodiscard]] Mode mode() const;
    /// Whether `start` starts or attaches to a client at all; false in a hidden run.
    [[nodiscard]] bool startsClient() const noexcept {
        return options_.mayStart;
    }

    /// Steam's installer, driven through the running client.
    [[nodiscard]] InstallWizard& installer() noexcept {
        return installer_;
    }
    /// Steam's uninstaller, driven through the running client.
    [[nodiscard]] Uninstaller& uninstaller() noexcept {
        return uninstaller_;
    }
    /// Big Picture, closed through the running client.
    [[nodiscard]] BigPicture& bigPicture() noexcept {
        return bigPicture_;
    }
    /// Steam's library folders, through the running client.
    [[nodiscard]] Libraries& libraries() noexcept {
        return libraries_;
    }
    /// Pausing, resuming, reordering and removing Steam's downloads, through the running client.
    [[nodiscard]] DownloadControl& downloadControl() noexcept {
        return downloadControl_;
    }
    /// True once the app's manifest says it is fully installed with no update pending.
    [[nodiscard]] bool installed(std::string_view appId) const;
    /// True while any library holds the app's manifest, which Steam deletes when it uninstalls.
    [[nodiscard]] bool hasManifest(std::string_view appId) const;
    /// Steam's unfinished downloads as last read, at most a second old while Steam is ready.
    [[nodiscard]] std::vector<Download> downloads() const;

    [[nodiscard]] launch::SteamState state() const override;
    [[nodiscard]] launch::SteamState waitReady(std::chrono::milliseconds timeout,
                                               const std::function<bool()>& cancelled) override;
    [[nodiscard]] std::optional<double> updateProgress(std::string_view appId) const override;
    [[nodiscard]] launch::SteamAppActivity activity(std::string_view appId) const override;
    void cancelLaunch(std::string_view appId) override;
    [[nodiscard]] bool showsInSession() const override;

  private:
    /// Managed and Isolated: Steam in openSU's scope, sandboxed with `devices` when Isolated.
    void startOwn(Mode mode, const SandboxDevices& devices);
    /// Wraps `command` and `args` in Isolated's bwrap and names the proxy for its Secret Service;
    /// false, logged, when bwrap or the proxy is missing or there is no Gamescope display.
    [[nodiscard]] bool sandbox(const SandboxDevices& devices, std::string& command,
                               std::vector<std::string>& args, std::filesystem::path& proxy) const;
    void startNormal();
    /// Whether the client watched is there: the scope, or the desktop's Steam process.
    [[nodiscard]] bool clientAlive();
    void beginWatching(launch::SteamState state);
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
    OrphanedSteam orphans_;
    std::filesystem::path program_;
    std::filesystem::path connectionLog_;
    /// How much of the connection log predates this client; only later lines count.
    std::uintmax_t logOffset_{0};
    launch::Instance instance_;
    /// Isolated only: the Secret Service relay; ended after the client.
    SecretBus secretBus_;
    DevTools devTools_{options_.devToolsPort};
    DownloadQueue queue_{devTools_};
    InstallWizard installer_{devTools_};
    Uninstaller uninstaller_{devTools_};
    BigPicture bigPicture_{devTools_};
    LaunchActivity launches_{devTools_};
    Libraries libraries_{devTools_};
    DownloadControl downloadControl_{devTools_};

    mutable std::mutex mutex_;
    std::condition_variable changed_;
    launch::SteamState state_{launch::SteamState::Stopped};
    Mode mode_{Mode::Managed};
    bool stopping_{false};
    std::vector<Download> downloads_;
    Activities activities_;
    /// The last reason the queue could not be read, logged once per change.
    std::string queueError_;
    std::thread watcher_;
};

} // namespace opensu::steam
