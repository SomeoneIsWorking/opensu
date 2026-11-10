// dolphin_config — Dolphin's launch rule: its pad settings bound to the virtual pads for a launch
// openSU starts. Dolphin binds a slot to a device by `Device = <source>/<n>/<name>`, `n` counting
// devices of that source and name, in `GCPadNew.ini [GCPadN]`, `WiimoteNew.ini [WiimoteN]` and the
// `[Profile]` of a per-game profile (`GameSettings/<ID>.ini [Controls] PadProfileN` or
// `WiimoteProfileN`, a name under `Profiles/GCPad` or `Profiles/Wiimote`, or a folder of them). A
// slot or profile on an SDL pad is moved to the Nth virtual pad, its mappings kept; a GameCube one
// on anything else (keyboard and mouse) gets a standard GameCube pad there. A Wii Remote on
// anything else is left as it is. Analytics' permission prompt is marked asked.
#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "emulator_launch_config.hpp"
#include "launch_files.hpp"

namespace opensu::library {

/// The catalog's Dolphin.
inline constexpr std::string_view dolphinId{"dolphin"};
/// The pads and Wii Remotes Dolphin emulates.
inline constexpr int dolphinSlots{4};

/// What a Dolphin controller section emulates.
enum class DolphinController : std::uint8_t {
    GameCube,
    WiiRemote,
};

/// `ini` with `[section]`, a `controller` slot, bound to the `player`th virtual pad (from 1).
[[nodiscard]] std::string dolphinSlotSettings(std::string_view ini, std::string_view section,
                                              DolphinController controller, int player);

/// The files the rule may set: the pad settings, `Dolphin.ini` and each per-game profile.
[[nodiscard]] std::vector<std::filesystem::path> dolphinFiles(const LaunchSetup& setup);

/// Those files as the launch reads them; none when the game reads the pads themselves.
[[nodiscard]] std::vector<RunFile> dolphinRunFiles(const LaunchSetup& setup);

} // namespace opensu::library
