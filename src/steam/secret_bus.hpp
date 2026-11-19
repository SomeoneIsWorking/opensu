// secret_bus — the one piece of the desktop's session bus Isolated Steam gets: an
// `xdg-dbus-proxy` in a scope of its own that lets through only the Secret Service
// (org.freedesktop.secrets), where Steam keeps its login, and the names Steam Runtime's launcher
// service owns, without which the SteamRT3 client exits at start. Without it Steam either never
// logs on (no bus) or autolaunches a private bus with a second secret service on the player's
// wallet beside the desktop's.
#pragma once

#include <chrono>
#include <filesystem>
#include <string>
#include <string_view>

#include "launch/process_launcher.hpp"

namespace opensu::steam {

/// The names `steam-runtime-launcher-service` takes, one per Steam instance; refused, it exits 69
/// and Steam with it.
inline constexpr std::string_view launcherServiceNames =
    "com.steampowered.PressureVessel.LaunchAlongsideSteam.*";

/// How long `start` waits for the proxy's socket.
inline constexpr std::chrono::seconds secretBusWait{5};

class SecretBus {
  public:
    /// `containment` holds the proxy (`launch::chooseContainment`).
    explicit SecretBus(launch::Containment containment) : instance_{containment} {
    }

    /// Starts `proxy` (xdg-dbus-proxy) in `unit`, relaying `desktopBus` (a D-Bus address) to a
    /// socket at `socket`, filtered to org.freedesktop.secrets and `launcherServiceNames`; returns
    /// once the socket is there.
    /// False with `failure` set when it could not be started or did not listen in time.
    [[nodiscard]] bool start(const std::string& unit, const std::filesystem::path& proxy,
                             const std::string& desktopBus, const std::filesystem::path& socket,
                             std::string& failure);
    /// Ends the proxy and removes its socket.
    void stop();

  private:
    launch::ProcessLauncher instance_;
    std::filesystem::path socket_;
};

} // namespace opensu::steam
