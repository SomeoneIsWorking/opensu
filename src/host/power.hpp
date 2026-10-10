// power — suspend, restart and shut down through systemd-logind (`systemctl`). One interface so
// tests and hidden runs use a backend that never touches the machine. Leaving for the desktop and
// back is `session_mode`.
#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "runner.hpp"

namespace opensu::host {

enum class PowerAction : std::uint8_t { Suspend, Restart, ShutDown };

/// The action's name for menus and messages ("Sleep", "Restart", "Shut down").
[[nodiscard]] const char* label(PowerAction action) noexcept;

class Power {
  public:
    virtual ~Power() = default;

    /// Asks the system to do `action`. Empty when it accepted, else why it refused.
    virtual std::string perform(PowerAction action) = 0;
};

/// `systemctl suspend|reboot|poweroff`, which logind authorises for the active session.
[[nodiscard]] std::unique_ptr<Power> makeLogindPower(Runner run);

/// A backend that refuses every action with `reason`; for hidden runs.
[[nodiscard]] std::unique_ptr<Power> makeRefusingPower(std::string reason);

} // namespace opensu::host
