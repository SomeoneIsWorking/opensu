// rpcs3_config — RPCS3's launch rule: its input profiles and `GuiConfigs/CurrentSettings.ini` set
// for a launch openSU starts. The welcome dialog, which opens even with `--no-gui` and takes a
// mouse to close, is off. RPCS3's SDL handler binds a player by name, `Device: <SDL name> <k>`, k
// counting pads of that name from 1, and a launch lets SDL see only the virtual pads, so player N
// plays `Xbox 360 Controller N`: an SDL player keeps its mappings with only `Device` retargeted;
// player 1, or any player on another handler (keyboard, evdev, a DualShock read over hidraw), is
// given RPCS3's own SDL layout. Every profile under `input_configs/<folder>/` is bound, and
// `global/Default.yml`, which RPCS3 falls back to, is made when the player has none.
#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "emulator_launch_config.hpp"
#include "launch_files.hpp"

namespace opensu::library {

/// The catalog's RPCS3.
inline constexpr std::string_view rpcs3Id{"rpcs3"};
/// Its input profiles' root, and the fallback profile, under its settings folder.
inline constexpr std::string_view rpcs3InputConfigs{"input_configs"};
inline constexpr std::string_view rpcs3DefaultProfile{"input_configs/global/Default.yml"};
/// Its interface settings file under its settings folder.
inline constexpr std::string_view rpcs3GuiSettings{"GuiConfigs/CurrentSettings.ini"};
/// The players an input profile binds.
inline constexpr int rpcs3Players{7};

/// The input profile `yml` with its players bound to the virtual pads.
[[nodiscard]] std::string rpcs3PadProfile(std::string_view yml);

/// The interface settings `ini` with the welcome dialog off.
[[nodiscard]] std::string rpcs3GuiLaunchSettings(std::string_view ini);

/// The files the rule may set under `setup.config`.
[[nodiscard]] std::vector<std::filesystem::path> rpcs3Files(const LaunchSetup& setup);

/// The interface settings and, on the virtual pads, every input profile as the launch reads them.
[[nodiscard]] std::vector<RunFile> rpcs3RunFiles(const LaunchSetup& setup);

} // namespace opensu::library
