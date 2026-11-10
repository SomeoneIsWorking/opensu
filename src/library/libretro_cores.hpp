// libretro_cores — the libretro cores built-in emulators run. A core is found in openSU's
// downloads under `<data>/opensu/cores/`, else in RetroArch's and the system's core folders. A
// download comes from the libretro buildbot's nightly folder over the folder's own origin only,
// is checked against the CRC-32 the folder's `.index-extended` lists, and is recorded beside the
// core (`<core>.json`: build date, CRC-32, URL) so a newer build can be told apart. Its libretro
// core info (`<core>.info`, which lists its BIOS files) comes from the buildbot's info archive with
// it and is kept beside it; a core installed another way has its info in RetroArch's folders. A
// core the catalog builds itself comes from its release instead (`installRelease`), checked against
// the SHA-256 the release publishes, with its info still from the buildbot.
#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>
#include <vector>

#include "emulator_release.hpp"
#include "net/web_client.hpp"

namespace opensu::library::roms {

/// The folder's listing: one "YYYY-MM-DD <crc32 hex> <file>" line per build.
inline constexpr std::string_view coreIndexFile = ".index-extended";

/// One core's build as the buildbot lists it.
struct CoreBuild {
    std::string core;
    /// The build's date, which the player reads as its version.
    std::string date;
    /// The CRC-32 of the library inside the zip, as the buildbot lists it.
    std::uint32_t crc32{0};
    std::string url;

    bool operator==(const CoreBuild&) const = default;
};

/// The build of `core` in the listing `index` of the folder `base`. Nothing, with `error`, when
/// it is not listed or its line is malformed.
[[nodiscard]] std::optional<CoreBuild> coreBuildIn(std::string_view index, std::string_view core,
                                                   std::string_view base, std::string& error);

/// A core openSU downloaded, as its record says.
struct DownloadedCore {
    std::string core;
    std::filesystem::path file;
    std::string date;
    std::uint32_t crc32{0};
    std::string url;

    bool operator==(const DownloadedCore&) const = default;
};

/// The cores openSU downloaded: `<core><suffix>` (`coreTarget`) and its record `<core>.json` in one
/// folder.
class DownloadedCores {
  public:
    explicit DownloadedCores(std::filesystem::path root) : root_{std::move(root)} {
    }
    [[nodiscard]] static DownloadedCores under(const std::filesystem::path& dataDir);

    [[nodiscard]] const std::filesystem::path& root() const noexcept {
        return root_;
    }

    /// The record of `core`, or nothing when openSU did not download it or its file is gone.
    [[nodiscard]] std::optional<DownloadedCore> find(std::string_view core) const;

    /// Replaces `build.core`'s file with `library` whole and its info with `info`, then records
    /// `build`. False with `error` when a file cannot be written; the core before stays then.
    [[nodiscard]] bool install(const CoreBuild& build, std::string_view library,
                               std::string_view info, std::string& error) const;

    /// Writes `core`'s info file whole. False with `error` when it cannot be written.
    [[nodiscard]] bool installInfo(std::string_view core, std::string_view info,
                                   std::string& error) const;

    /// Removes `core`, its info and its record. False with `error` when something stays behind.
    [[nodiscard]] bool remove(std::string_view core, std::string& error) const;

    /// Where `core`'s file goes.
    [[nodiscard]] std::filesystem::path file(std::string_view core) const;
    /// Where `core`'s info file goes.
    [[nodiscard]] std::filesystem::path info(std::string_view core) const;

  private:
    [[nodiscard]] std::filesystem::path record(std::string_view core) const;

    std::filesystem::path root_;
};

/// A core found on this machine.
struct FoundCore {
    std::filesystem::path file;
    /// openSU's download; nothing for a core installed another way.
    std::optional<DownloadedCore> downloaded;
};

/// Where cores are looked for: openSU's downloads, then each folder in order.
struct CoreSearch {
    /// openSU's downloads; empty for none.
    std::filesystem::path downloads;
    std::vector<std::filesystem::path> folders;
    /// Where the info files of cores installed another way are, after openSU's downloads.
    std::vector<std::filesystem::path> infoFolders;

    /// openSU's downloads under `dataDir`, then the folders RetroArch and the system install cores to on this OS (`core_search_linux.cpp`:
    /// ~/.config/retroarch/cores, /usr/lib64/libretro, /usr/lib/libretro and
    /// /usr/lib/x86_64-linux-gnu/libretro, with ~/.config/retroarch/info and
    /// /usr/share/libretro/info; `core_search_none.cpp`: none).
    [[nodiscard]] static CoreSearch standard(const std::filesystem::path& home,
                                             const std::filesystem::path& dataDir);

    [[nodiscard]] std::optional<FoundCore> find(std::string_view core) const;
    /// `core`'s info file text, or nothing when none is found.
    [[nodiscard]] std::optional<std::string> info(std::string_view core) const;
};

class CoreDownload {
  public:
    /// Downloads from the buildbot folder `base`, which ends in /.
    CoreDownload(const net::WebClient& web, std::string base) : web_{web}, base_{std::move(base)} {
    }

    /// The folder's listing.
    [[nodiscard]] net::Result<std::string> index(const std::stop_token& stop) const;

    /// The latest build of `core`.
    [[nodiscard]] net::Result<CoreBuild> latest(const std::stop_token& stop,
                                                std::string_view core) const;

    /// The buildbot's archive of every core's info file, as zip bytes.
    [[nodiscard]] net::Result<std::string> infoArchive(const std::stop_token& stop) const;

    /// Downloads `build` and its info, checks the zip against its CRC-32, takes the one core out
    /// of it and switches to it. Unless it succeeds, a stop or cancel included, nothing of the
    /// download stays and the core in use stays.
    [[nodiscard]] net::Result<DownloadedCore>
    install(const std::stop_token& stop, const CoreBuild& build, const DownloadedCores& store,
            const net::WebClient::Progress& progress) const;

    /// Downloads `core` as the release's `asset`, checks it against the SHA-256 the release
    /// publishes (refused when it publishes none), takes the one core out of its zip and switches
    /// to it, recording the release's version as its date. As `install`, nothing stays unless it
    /// succeeds.
    [[nodiscard]] net::Result<DownloadedCore>
    installRelease(const std::stop_token& stop, const std::string& core, const ReleaseAsset& asset,
                   const EmulatorRelease& release, const DownloadedCores& store,
                   const net::WebClient::Progress& progress) const;

    [[nodiscard]] const std::string& base() const noexcept {
        return base_;
    }

  private:
    /// Why `url` may not be fetched: every hop stays on the folder's own origin.
    [[nodiscard]] std::string refusal(const std::string& url) const;
    /// `core`'s info file from the buildbot's archive.
    [[nodiscard]] net::Result<std::string> infoOf(const std::stop_token& stop,
                                                  const std::string& core) const;
    /// Puts `library`, taken from the zip `bytes`, and `info` in `store` as `build`; `build.crc32`
    /// is checked when `checkCrc`, else set from the library.
    [[nodiscard]] static net::Result<DownloadedCore> place(CoreBuild build, std::string bytes,
                                                           const std::string& info, bool checkCrc,
                                                           const DownloadedCores& store);

    const net::WebClient& web_;
    std::string base_;
};

} // namespace opensu::library::roms
