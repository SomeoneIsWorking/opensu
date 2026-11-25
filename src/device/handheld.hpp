// handheld — which handheld this machine is, from its firmware's product name, for what only that
// handheld needs.
#pragma once

#include <filesystem>
#include <string_view>

namespace opensu::device {

/// Where the firmware's identity is read on Linux.
inline constexpr std::string_view dmiDir{"/sys/class/dmi/id"};

/// Whether `dmi` names an ASUS ROG Ally (the Ally and the Ally X); false where it cannot be read.
[[nodiscard]] bool rogAlly(const std::filesystem::path& dmi = std::filesystem::path{dmiDir});

} // namespace opensu::device
