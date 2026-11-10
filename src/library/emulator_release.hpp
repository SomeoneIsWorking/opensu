// emulator_release — an emulator's latest official AppImage: read from its release API (GitHub's
// or Forgejo's release JSON), downloaded from the project's own host, checked against the
// release's SHA-256 when it publishes one, taken out of the zip when the release ships it in one
// (`ReleaseSource::member`), and handed to `DownloadedEmulators` to switch to. A core
// published as a release (`EmulatorEntry::release` of a built-in one) is fetched the same way.
#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>

#include "downloaded_emulators.hpp"
#include "emulator_catalog.hpp"
#include "net/web_client.hpp"

namespace opensu::library::roms {

/// The asset of a release that the catalog downloads.
struct ReleaseAsset {
    /// The release's tag, with its date when the tag is a rolling name ("latest").
    std::string version;
    std::string assetId;
    std::string name;
    std::string url;
    /// The size the release states; 0 when it states none.
    std::uint64_t size{0};
    /// The SHA-256 the release publishes, lower-case hex; empty when it publishes none.
    std::string sha256;

    bool operator==(const ReleaseAsset&) const = default;
};

/// The asset of `source` in the release JSON `body`. Nothing, with `error`, when no asset matches,
/// two do, or its URL is not on the project's own host.
[[nodiscard]] std::optional<ReleaseAsset>
releaseAssetOf(std::string_view body, const ReleaseSource& source, std::string& error);

class EmulatorRelease {
  public:
    explicit EmulatorRelease(const net::WebClient& web) : web_{web} {
    }

    /// The latest release's asset for `source`.
    [[nodiscard]] net::Result<ReleaseAsset> latest(const std::stop_token& stop,
                                                   const ReleaseSource& source) const;

    /// Downloads `asset` to `file` and checks its size and SHA-256 against the release's, returning
    /// the SHA-256 it has. Unless it succeeds, a stop or cancel included, `file` is removed. Every
    /// hop `refusal` refuses fails it.
    [[nodiscard]] net::Result<std::string> fetch(const std::stop_token& stop,
                                                 const ReleaseAsset& asset,
                                                 const std::filesystem::path& file,
                                                 const net::WebClient::Progress& progress,
                                                 const net::WebClient::Refusal& refusal = {}) const;

    /// Downloads `asset` of `entry` beside the version in use, verifies it, takes the AppImage out
    /// of it when `entry`'s release names a zip member, makes it executable and switches to it. The
    /// record's size and SHA-256 are the download's. Unless it succeeds, a stop or cancel included,
    /// the partial file is removed and the version in use stays.
    [[nodiscard]] net::Result<DownloadedEmulator>
    install(const std::stop_token& stop, const EmulatorEntry& entry, const ReleaseAsset& asset,
            const DownloadedEmulators& store, const net::WebClient::Progress& progress) const;

  private:
    const net::WebClient& web_;
};

} // namespace opensu::library::roms
