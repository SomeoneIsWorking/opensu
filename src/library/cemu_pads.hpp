// cemu_pads — Cemu's launch rule: its controller profiles bound to the virtual pads a game openSU
// launched reads. Cemu binds `controllerProfiles/controller<N>.xml` to an SDL pad by its uuid,
// `<index>_<SDL GUID>`, the index counting pads of that GUID in SDL's order (the event number),
// so a profile made with the player's own pad matches none of openSU's virtual pads. Each profile
// with an SDL controller is bound to the Nth virtual pad, its mappings kept.
#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "emulator_launch_config.hpp"
#include "launch_files.hpp"

namespace opensu::library {

/// The catalog's Cemu.
inline constexpr std::string_view cemuId{"cemu"};
/// Cemu's emulated controllers.
inline constexpr int cemuControllers{8};

/// `profile` with every SDL controller's uuid made `uuid`; the rest byte for byte.
[[nodiscard]] std::string reboundCemuProfile(std::string_view profile, std::string_view uuid);

/// The files Cemu's rule may set under its settings folder.
[[nodiscard]] std::vector<std::filesystem::path> cemuFiles(const LaunchSetup& setup);

/// Each controller profile under the settings folder bound to the virtual pads; none when the
/// game reads the pads themselves (no pad GUID) or a profile has no SDL controller.
[[nodiscard]] std::vector<RunFile> cemuRunFiles(const LaunchSetup& setup);

} // namespace opensu::library
