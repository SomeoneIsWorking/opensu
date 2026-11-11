// xemu_config — xemu's launch rule: `xemu.toml` set for a launch openSU starts. Its first-boot
// welcome window is off. xemu binds a port to a pad by SDL's joystick GUID, `[input.bindings]
// portN`, or to the keyboard by `keyboard`; a pad takes the first free port that lists its GUID,
// and pads of one GUID share the ports listing it (`xemu-input.c`). The virtual pads share one
// GUID, so every port is given it: pad N takes port N, and the keyboard, which xemu binds before
// any pad, takes none.
#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "emulator_launch_config.hpp"
#include "launch_files.hpp"

namespace opensu::library {

/// The catalog's xemu.
inline constexpr std::string_view xemuId{"xemu"};
/// Its settings file under its settings folder.
inline constexpr std::string_view xemuSettingsFile{"xemu.toml"};
/// The controller ports xemu binds, `port1` to `port4`.
inline constexpr int xemuPorts{4};

/// The settings file `toml` as a launch reads it: the welcome window off and, with `padGuid` not
/// empty, every port on the virtual pads.
[[nodiscard]] std::string xemuLaunchSettings(std::string_view toml, std::string_view padGuid);

/// The files the rule may set under `setup.config`.
[[nodiscard]] std::vector<std::filesystem::path> xemuFiles(const LaunchSetup& setup);

/// The settings file as the launch reads it, made when the player has none.
[[nodiscard]] std::vector<RunFile> xemuRunFiles(const LaunchSetup& setup);

} // namespace opensu::library
