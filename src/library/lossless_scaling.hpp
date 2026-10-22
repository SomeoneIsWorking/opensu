// lossless_scaling — the player's own Lossless Scaling install from Steam (app 993090), whose
// `Lossless.dll` the Gamescope fork's frame generator reads its shaders from. Found in place in
// any of the player's Steam libraries, read only, never copied.
#pragma once

#include <filesystem>
#include <optional>
#include <string_view>

#include "steam.hpp"

namespace opensu::library {

/// Where the DLL sits in a Steam library folder.
inline constexpr std::string_view losslessDllInLibrary =
    "steamapps/common/Lossless Scaling/Lossless.dll";

/// `Lossless.dll` in the first of `steam`'s library folders that holds it, or nothing.
[[nodiscard]] std::optional<std::filesystem::path> findLosslessDll(const steam::Library& steam);

} // namespace opensu::library
