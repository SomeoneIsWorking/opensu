// host_services — the machine's own services the shell reaches: power (logind), Bluetooth (BlueZ)
// and the backlight. One place decides what a hidden run may touch: it may read the Bluetooth
// state and the brightness, and it never sleeps, restarts, shuts down, scans, pairs, connects,
// forgets or writes a brightness.
#pragma once

#include <filesystem>
#include <memory>

#include "config/config.hpp"
#include "host/backlight.hpp"
#include "host/bluetooth.hpp"
#include "host/power.hpp"

namespace opensu::app {

struct HostServices {
    std::unique_ptr<host::Power> power;
    std::unique_ptr<host::Bluetooth> bluetooth;
    /// Null on a machine without a backlight device.
    std::unique_ptr<host::Backlight> backlight;

    /// The real services for this machine; with `hidden` they can be read but not changed.
    [[nodiscard]] static HostServices detect(const config::Config& config, bool hidden);

    /// The services over `run` and `spawn`, with the backlight under `backlightRoot` and `exit`
    /// leaving the login session.
    [[nodiscard]] static HostServices over(const host::Runner& run, const host::Spawner& spawn,
                                           const std::filesystem::path& backlightRoot, bool hidden,
                                           const host::SessionExit& exit = {});
};

} // namespace opensu::app
