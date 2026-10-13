// emulator_release — an emulator's latest official AppImage: read from its release API (GitHub's
// or Forgejo's release JSON), downloaded from the project's own host, checked against the
// release's SHA-256 when it publishes one, and handed to `DownloadedEmulators` to switch to.
#pragma once

#include <cstdint>
#include <optional>
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
    [[nodiscard]] std::optional<ReleaseAsset> latest(const ReleaseSource& source,
                                                     std::string& error) const;

    /// Downloads `asset` of `entry` beside the version in use, verifies it, makes it executable
    /// and switches to it. On any failure, a cancel included, the partial file is removed, the
    /// version in use stays, and `error` says why.
    [[nodiscard]] std::optional<DownloadedEmulator>
    install(const EmulatorEntry& entry, const ReleaseAsset& asset, const DownloadedEmulators& store,
            const net::WebClient::Progress& progress, std::string& error) const;

  private:
    const net::WebClient& web_;
};

} // namespace opensu::library::roms
