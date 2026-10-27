// runtime_archive — one release tarball openSU unpacks into a folder it owns (a GE-Proton build,
// umu-launcher): downloaded beside the target, its size and SHA-256 or SHA-512 checked against
// what the release publishes, unpacked with `tar` into a staging folder, and its one top folder
// moved into place. A failure or cancel leaves nothing behind and what was installed before as it
// was.
#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <stop_token>
#include <string>

#include "net/web_client.hpp"

namespace opensu::library {

struct RuntimeArchive {
    enum class Digest : std::uint8_t { Sha256, Sha512 };

    std::string url;
    /// The file name the release gives it.
    std::string name;
    /// The size the release states; 0 when it states none.
    std::uint64_t size{0};
    Digest digest{Digest::Sha512};
    /// The published digest, lower-case hex.
    std::string hex;
    /// The one folder the archive holds.
    std::string folder;
    /// What it is named in place, `<into>/<name>`: the release's build name.
    std::string installAs;
};

/// Where an install stands.
enum class ArchiveStage : std::uint8_t { Downloading, Checking, Unpacking };

/// Downloads `archive` and unpacks its folder to `<into>/<archive.installAs>`; staging files
/// live in `<into>/.partial`. `progress` sees the bytes and may cancel; `stage` is told each step.
/// The installed folder, or nothing with `error` ("cancelled" for a stop).
[[nodiscard]] std::optional<std::filesystem::path>
installArchive(const net::WebClient& web, const RuntimeArchive& archive,
               const std::filesystem::path& into, const std::stop_token& stop,
               const net::WebClient::Progress& progress,
               const std::function<void(ArchiveStage)>& stage, std::string& error);

} // namespace opensu::library
