// steam_compat — the compatibility tool Steam runs each installed game under, read only from
// Steam's own records, in Steam's order: the player's pick per game (`config/config.vdf`
// CompatToolMapping), else Valve's mapping for it (app 891390's `app_mappings`), else for a
// Windows-only game the player's default (CompatToolMapping's `0`); each game's platforms and
// Steam's tools with their aliases (`proton-stable`) from `steam_appinfo`, and where each tool is
// installed (a `compatibilitytools.d` tool's `compatibilitytool.vdf`, an official Proton's app
// manifest).
#pragma once

#include <filesystem>
#include <functional>
#include <map>
#include <string>
#include <vector>

#include "steam.hpp"

namespace opensu::library::steam {

/// The app whose info lists Steam's compatibility tools by name and Valve's per-game mappings.
inline constexpr std::string_view toolRegistryApp = "891390";

struct CompatTool {
    /// Steam's name for it, an alias resolved: `proton_10`, `GE-Proton11-5-x86_64`.
    std::string name;
    /// Its folder; empty when it is not installed.
    std::filesystem::path dir;
};

/// The tool Steam runs each of `appIds` under, by app id; a game Steam runs as it is is absent.
[[nodiscard]] std::map<std::string, CompatTool, std::less<>>
compatTools(const Library& library, const std::vector<std::string>& appIds);

} // namespace opensu::library::steam
