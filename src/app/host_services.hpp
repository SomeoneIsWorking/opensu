// host_services — the machine's own services the shell reaches: power (logind), session mode
// (SDDM), Bluetooth (BlueZ) and the backlight. One place decides what a hidden run may touch: it
// may read the Bluetooth state, the brightness and whether session mode is installed, and it never
// sleeps, restarts, shuts down, installs or switches session mode, scans, pairs, connects, forgets
// or writes a brightness.
#pragma once

#include <filesystem>
#include <memory>

#include "config/config.hpp"
#include "host/backlight.hpp"
#include "host/bluetooth.hpp"
#include "host/power.hpp"
#include "host/session_mode.hpp"

namespace opensu::app {

struct HostServices {
    std::unique_ptr<host::Power> power;
    /// Installing session mode and switching between it and the desktop.
    std::unique_ptr<host::SessionMode> session;
    std::unique_ptr<host::Bluetooth> bluetooth;
    /// Null on a machine without a backlight device.
    std::unique_ptr<host::Backlight> backlight;

    /// The real services for this machine; with `hidden` they can be read but not changed.
    [[nodiscard]] static HostServices detect(const config::Config& config, bool hidden);

    /// The services over `run`, `runLine` and `spawn`, with the backlight under `backlightRoot`
    /// and session mode found by `paths`.
    [[nodiscard]] static HostServices over(const host::Runner& run, const host::LineRunner& runLine,
                                           const host::Spawner& spawn,
                                           const std::filesystem::path& backlightRoot, bool hidden,
                                           host::SessionPaths paths = {});
};

} // namespace opensu::app
