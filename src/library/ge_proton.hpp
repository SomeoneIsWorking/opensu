// ge_proton — GE-Proton as openSU's own Wine runner: GloriousEggroll's releases on GitHub (the
// tarball for this machine and its published `.sha512sum`), the builds openSU unpacked under
// `<data>/wine/runners/GE-Proton<major>-<minor>`, their order by version, and which of them a
// rotation removes: the newest two stay, and any build a running game or the player's pick uses.
// Nothing outside openSU's runners folder is ever listed or removed.
#pragma once

#include <compare>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "emulator_catalog.hpp"

namespace opensu::library::ge_proton {

/// The latest release's JSON.
inline constexpr std::string_view latestApi =
    "https://api.github.com/repos/GloriousEggroll/proton-ge-custom/releases/latest";
/// Every asset must be downloaded from here.
inline constexpr std::string_view downloads =
    "https://github.com/GloriousEggroll/proton-ge-custom/releases/download/";
/// How many builds a rotation keeps: the newest and the one before it.
inline constexpr std::size_t kept = 2;

/// `GE-Proton<major>-<minor>`.
struct Version {
    int major{0};
    int minor{0};

    auto operator<=>(const Version&) const = default;
};

/// The version a build or tag is named for; nothing for any other name.
[[nodiscard]] std::optional<Version> versionOf(std::string_view name);

/// openSU's runners folder under its data dir.
[[nodiscard]] std::filesystem::path runnersDir(const std::filesystem::path& dataDir);

/// The release source of the tarball for this machine (`-x86_64` or `-aarch64`, or a release from
/// before GE published both) from `api` (empty for the latest release).
[[nodiscard]] roms::ReleaseSource tarballSource(std::string_view api);
/// The release source of the tarball's `.sha512sum`.
[[nodiscard]] roms::ReleaseSource checksumSource(std::string_view api);

/// The SHA-512 that `listing` (`sha512sum` output) gives for `file`, lower-case; nothing when it
/// lists no such file or the digest is not 128 hex digits.
[[nodiscard]] std::optional<std::string> sha512Listed(std::string_view listing,
                                                      std::string_view file);

/// One build openSU installed.
struct Build {
    std::string name;
    Version version;
    std::filesystem::path dir;

    bool operator==(const Build&) const = default;
};

/// The builds in `runners` with a `proton` script and a Wine binary, newest first.
[[nodiscard]] std::vector<Build> installed(const std::filesystem::path& runners);

/// Whether `dir` is one of openSU's builds under `runners`.
[[nodiscard]] bool managed(const std::filesystem::path& runners, const std::filesystem::path& dir);

/// The builds of `newestFirst` a rotation removes: all past the newest `kept`, except those in
/// `inUse`.
[[nodiscard]] std::vector<Build> rotation(const std::vector<Build>& newestFirst,
                                          const std::set<std::filesystem::path>& inUse);

} // namespace opensu::library::ge_proton
