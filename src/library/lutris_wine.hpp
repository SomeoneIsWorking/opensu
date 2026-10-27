// lutris_wine — what Lutris runs a Windows game under, read from the player's Lutris folders and
// never written. Lutris (0.5.18 on) defaults to "ge-proton": umu-run with `PROTONPATH=GE-Proton`,
// the newest GE-Proton in umu's `compatibilitytools.d`, which umu downloads when there is none.
// `runners/wine.yml` can name another version: a build in `runners/wine`, a Proton build by name,
// or a system Wine ("system", "winehq-staging" and the like).
#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace opensu::library::lutris {

/// Lutris's name for "the newest GE-Proton, through umu".
inline constexpr std::string_view geProtonLatest = "ge-proton";

/// Lutris's data folder under the XDG data home.
[[nodiscard]] std::filesystem::path dataDir(const std::filesystem::path& xdgDataHome);

/// The Wine builds Lutris downloaded: `runners/wine/<name>`.
[[nodiscard]] std::filesystem::path wineDir(const std::filesystem::path& lutrisData);

/// The Wine version Lutris uses for a game with no version of its own: `version` under `wine:` in
/// `runners/wine.yml`, else `geProtonLatest`.
[[nodiscard]] std::string defaultWineVersion(const std::filesystem::path& lutrisData);

/// The Wine binary of a system Wine Lutris names (`system`, `winehq-staging`, ...); nothing for
/// any other version.
[[nodiscard]] std::optional<std::filesystem::path> systemWineOf(std::string_view version);

/// Lutris's own `umu-run` under `runtime/umu`; empty when it has none.
[[nodiscard]] std::filesystem::path umuRun(const std::filesystem::path& lutrisData);

} // namespace opensu::library::lutris
