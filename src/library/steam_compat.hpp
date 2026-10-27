// steam_compat — the compatibility tool Steam runs each installed game under, read only from
// Steam's own records: the player's pick per game and the default for every other Windows-only game
// (`config/config.vdf` CompatToolMapping), each game's platforms and Steam's tool names
// (`steam_appinfo`), and where each tool is installed (a `compatibilitytools.d` tool's
// `compatibilitytool.vdf`, an official Proton's app manifest). Valve's own per-game mappings are
// not read.
#pragma once

#include <filesystem>
#include <functional>
#include <map>
#include <string>
#include <vector>

#include "steam.hpp"

namespace opensu::library::steam {

/// The app whose info lists Steam's compatibility tools by name.
inline constexpr std::string_view toolRegistryApp = "891390";

struct CompatTool {
    /// Steam's name for it: `proton_10`, `GE-Proton11-5-x86_64`.
    std::string name;
    /// Its folder; empty when it is not installed.
    std::filesystem::path dir;
};

/// The tool Steam runs each of `appIds` under, by app id; a game Steam runs as it is is absent.
[[nodiscard]] std::map<std::string, CompatTool, std::less<>>
compatTools(const Library& library, const std::vector<std::string>& appIds);

} // namespace opensu::library::steam
