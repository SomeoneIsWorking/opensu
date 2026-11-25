// host_services — the machine's own services the shell reaches: power (logind), session mode
// (SDDM), Bluetooth (BlueZ), the backlight, the power profile, the stick lights' helper and openSU's
// own polkit agent. One place decides what a hidden run may touch: it may read the Bluetooth state,
// the brightness, whether session mode or the lights' helper is installed and the power profile,
// and it never sleeps, restarts, shuts down, installs or switches session mode, installs or runs
// the lights' helper, scans, pairs, connects, forgets or writes a brightness or a power profile.
#pragma once

#include <filesystem>
#include <memory>
#include <string_view>

#include "config/config.hpp"
#include "host/backlight.hpp"
#include "host/bluetooth.hpp"
#include "host/polkit_agent.hpp"
#include "host/power.hpp"
#include "host/power_profiles.hpp"
#include "host/ring_light_helper.hpp"
#include "host/session_mode.hpp"

namespace opensu::app {

/// What a hidden run answers when asked to change the machine or what openSU installs.
inline constexpr std::string_view hiddenRunRefusal = "changes are off in a hidden run";

struct HostServices {
    std::unique_ptr<host::Power> power;
    /// Installing session mode and switching between it and the desktop.
    std::unique_ptr<host::SessionMode> session;
    std::unique_ptr<host::Bluetooth> bluetooth;
    /// Null on a machine without a backlight device.
    std::unique_ptr<host::Backlight> backlight;
    /// The power profile; null on a machine without PowerStation.
    std::unique_ptr<host::PowerProfiles> powerProfiles;
    /// The stick lights' colour where the kernel cannot carry it; null off Linux.
    std::unique_ptr<host::RingLightHelper> ringLights;
    /// The agent polkit asks for what openSU's children start through pkexec; null where there is
    /// no polkit, and in services made `over` fakes.
    std::unique_ptr<host::PolkitAgent> polkit;

    /// The real services for this machine; with `hidden` they can be read but not changed.
    [[nodiscard]] static HostServices detect(const config::Config& config, bool hidden);
    /// Where session mode is for the openSU `config` describes: the program its entry starts,
    /// the root step staged under the runtime dir, pkexec from PATH, a test run's stand-ins for
    /// the root step and for the root session mode is looked for under.
    [[nodiscard]] static host::SessionPaths sessionPaths(const config::Config& config);

    /// Where the stick lights' helper is: its root step staged under the runtime dir, pkexec from
    /// PATH and a test run's stand-in for the root step.
    [[nodiscard]] static host::RingLightHelperPaths ringLightPaths(const config::Config& config);

    /// The services over `run` and `spawn`, with the backlight under `backlightRoot`, session mode
    /// found by `paths` and the lights' helper by `lights`.
    [[nodiscard]] static HostServices over(const host::Runner& run, const host::Spawner& spawn,
                                           const std::filesystem::path& backlightRoot, bool hidden,
                                           host::SessionPaths paths = {},
                                           host::RingLightHelperPaths lights = {});
};

} // namespace opensu::app
