// secret_bus — the one piece of the desktop's session bus Isolated Steam gets: an
// `xdg-dbus-proxy` in a scope of its own that lets through only the Secret Service
// (org.freedesktop.secrets), where Steam keeps its login. Without it Steam either never logs on
// (no bus) or autolaunches a private bus with a second secret service on the player's wallet
// beside the desktop's.
#pragma once

#include <chrono>
#include <filesystem>
#include <string>

#include "launch/instance.hpp"

namespace opensu::steam {

/// How long `start` waits for the proxy's socket.
inline constexpr std::chrono::seconds secretBusWait{5};

class SecretBus {
  public:
    /// Starts `proxy` (xdg-dbus-proxy) in `unit`, relaying `desktopBus` (a D-Bus address) to a
    /// socket at `socket`, filtered to org.freedesktop.secrets; returns once the socket is there.
    /// False with `failure` set when it could not be started or did not listen in time.
    [[nodiscard]] bool start(const std::string& unit, const std::filesystem::path& proxy,
                             const std::string& desktopBus, const std::filesystem::path& socket,
                             std::string& failure);
    /// Ends the proxy and removes its socket.
    void stop();

  private:
    launch::Instance instance_;
    std::filesystem::path socket_;
};

} // namespace opensu::steam
