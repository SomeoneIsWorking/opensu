// containment — what holds a program a ProcessLauncher starts and everything it starts, chosen
// once per openSU process by its entry point: `containment_linux.cpp` on Linux,
// `containment_platform.cpp` elsewhere.
#pragma once

#include <cstdint>

namespace opensu::launch {

enum class Containment : std::uint8_t {
    /// A transient systemd user scope: Linux with a user manager that systemd-run reaches.
    systemdScope,
    /// The OS's own group: a Job Object on Windows, a process group on macOS, an intent on
    /// Android.
    platform,
};

/// The containment this process's launchers use, logged with the reason it was chosen.
[[nodiscard]] Containment chooseContainment();

} // namespace opensu::launch
