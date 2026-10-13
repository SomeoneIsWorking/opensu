// downloaded_emulators — the AppImages openSU downloaded, under `<data dir>/emulators/<id>/`.
// Each version sits in a folder of its release asset's id; `installed.json` names the one in use
// and how it was verified. Writing that record is the switch from one version to the next, so a
// version is in use only once it is whole and verified, and the one before stays until then.
#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace opensu::library::roms {

/// How a download's bytes were checked.
enum class Verification : std::uint8_t {
    /// Against the SHA-256 the release published.
    Digest,
    /// The release publishes none: fetched over HTTPS from the project's own host, and the size
    /// and SHA-256 seen are recorded.
    Observed,
};

struct DownloadedEmulator {
    std::string id;
    /// The release's tag, as the player reads it.
    std::string version;
    /// The release asset's id; a release that reuses a tag is a new asset.
    std::string assetId;
    std::filesystem::path file;
    std::string sha256;
    std::uint64_t size{0};
    Verification verified{Verification::Observed};
    std::string url;

    bool operator==(const DownloadedEmulator&) const = default;
};

class DownloadedEmulators {
  public:
    explicit DownloadedEmulators(std::filesystem::path root) : root_{std::move(root)} {
    }
    [[nodiscard]] static DownloadedEmulators under(const std::filesystem::path& dataDir);

    [[nodiscard]] const std::filesystem::path& root() const noexcept {
        return root_;
    }

    /// The version in use, or nothing when none was downloaded or its file is gone.
    [[nodiscard]] std::optional<DownloadedEmulator> find(std::string_view id) const;

    /// The folder a version with asset `assetId` is downloaded into.
    [[nodiscard]] std::filesystem::path versionDir(std::string_view id,
                                                   std::string_view assetId) const;

    /// Makes `record`, whose file is in place and verified, the version in use, then removes the
    /// other versions. False with `error` when the record cannot be written; the version before
    /// stays in use then.
    [[nodiscard]] bool install(const DownloadedEmulator& record, std::string& error) const;

    /// Removes every version of `id`. False with `error` when something stays behind.
    [[nodiscard]] bool remove(std::string_view id, std::string& error) const;

  private:
    [[nodiscard]] std::filesystem::path record(std::string_view id) const;

    std::filesystem::path root_;
};

} // namespace opensu::library::roms
