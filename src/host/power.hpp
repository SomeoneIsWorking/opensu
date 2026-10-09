// power — suspend, restart and shut down through systemd-logind (`systemctl`), and leaving a login
// session for the desktop. One interface so tests and hidden runs use a backend that never
// touches the machine.
#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "desktop_request.hpp"
#include "runner.hpp"

namespace opensu::host {

enum class PowerAction : std::uint8_t { Suspend, Restart, ShutDown, SwitchToDesktop };

/// The action's name for menus and messages ("Sleep", "Restart", "Shut down", "Switch to desktop").
[[nodiscard]] const char* label(PowerAction action) noexcept;

class Power {
  public:
    virtual ~Power() = default;

    /// Asks the system to do `action`. Empty when it accepted, else why it refused.
    virtual std::string perform(PowerAction action) = 0;
};

/// What leaving the login session needs from the machine.
struct SessionExit {
    /// The directories searched for `steamos-session-select`.
    std::vector<std::filesystem::path> path;
    /// Left for the login session, which starts the desktop once Gamescope has ended.
    DesktopRequest request{{}};
    /// The scope of the session's Gamescope, which ends it when `steamos-session-select` is absent.
    std::string compositorScope;
};

/// `systemctl suspend|reboot|poweroff`, which logind authorises for the active session.
/// SwitchToDesktop writes `exit.request`, then ends the session's Gamescope with
/// `steamos-session-select plasma` when that is on `exit.path`, else by stopping
/// `exit.compositorScope`. The login session then starts the desktop itself.
[[nodiscard]] std::unique_ptr<Power> makeLogindPower(Runner run, SessionExit exit = {});

/// A backend that refuses every action with `reason`; for hidden runs.
[[nodiscard]] std::unique_ptr<Power> makeRefusingPower(std::string reason);

} // namespace opensu::host
