// umu_launcher — umu-launcher, which runs a Proton build outside Steam inside the Steam Linux
// Runtime: its latest release's zipapp (`umu-launcher-<version>-zipapp.tar`, checked against the
// SHA-256 GitHub publishes for it) unpacked under `<data>/wine/umu-launcher/<version>`, and the
// newest `umu-run` there. Only the newest stays after an install.
#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "emulator_catalog.hpp"

namespace opensu::library::umu {

inline constexpr std::string_view latestApi =
    "https://api.github.com/repos/Open-Wine-Components/umu-launcher/releases/latest";
inline constexpr std::string_view downloads =
    "https://github.com/Open-Wine-Components/umu-launcher/releases/download/";

/// The latest release's zipapp.
[[nodiscard]] roms::ReleaseSource zipappSource();

/// openSU's umu-launcher folder under its data dir.
[[nodiscard]] std::filesystem::path launcherDir(const std::filesystem::path& dataDir);

/// One unpacked release.
struct Install {
    std::string version;
    std::filesystem::path umuRun;
};

/// The releases unpacked in `dir` with their `umu/umu-run`, newest first.
[[nodiscard]] std::vector<Install> installed(const std::filesystem::path& dir);

/// The Steam Linux Runtime umu fetched into `folders` (`wine::umuFolders`), or empty.
[[nodiscard]] std::filesystem::path steamRuntime(const std::filesystem::path& folders);

} // namespace opensu::library::umu
