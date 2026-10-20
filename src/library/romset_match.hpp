// romset_match — which arcade core an arcade zip's romset is made for. A zip's central directory
// (names, sizes, CRC-32s; nothing inflated) is compared with each core's listing (`RomsetDats`):
// the set of the zip's name matches a core when every ROM it needs is in the zip, in the zips of
// its parent and BIOS, or in its devices' zips, all in the same folder, and its disks are there.
// Verdicts are kept under `<cache>/romsets/matches.json` by the size and modification time of
// the zip and of every file the match looked at, so a library scan reads each zip once.
#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "emulator_catalog.hpp"
#include "game.hpp"
#include "romset_dat.hpp"
#include "romset_dats.hpp"

namespace opensu::library::roms {

/// One member of a zip as its central directory lists it.
struct ZipMember {
    std::string name;
    std::uint64_t size{0};
    std::uint32_t crc32{0};
};

/// The members of `zip`, read from its central directory only. Nothing, with `error`, when it
/// is not a zip that can be read (zip64 included).
[[nodiscard]] std::optional<std::vector<ZipMember>> readZipMembers(const std::filesystem::path& zip,
                                                                   std::string& error);

/// A file as it was when a match looked at it; `size` -1 when it was not there.
struct FileStamp {
    std::string path;
    std::int64_t size{-1};
    std::int64_t modified{0};

    [[nodiscard]] static FileStamp of(const std::filesystem::path& file);
    bool operator==(const FileStamp&) const = default;
};

/// What the zips of one folder hold, each read once, and every file looked at.
class FolderZips {
  public:
    explicit FolderZips(std::filesystem::path folder) : folder_{std::move(folder)} {
    }

    /// The members of `<set>.zip`; null when it is not there or cannot be read.
    [[nodiscard]] const std::vector<ZipMember>* members(std::string_view set);
    /// Whether `<set>/<disk>.chd` is there.
    [[nodiscard]] bool hasDisk(std::string_view set, std::string_view disk);

    [[nodiscard]] const std::vector<FileStamp>& looked() const noexcept {
        return looked_;
    }

  private:
    [[nodiscard]] std::filesystem::path zipOf(std::string_view set) const;

    std::filesystem::path folder_;
    std::map<std::string, std::optional<std::vector<ZipMember>>, std::less<>> zips_;
    std::vector<FileStamp> looked_;
};

/// One core's verdict on one zip.
struct RomsetVerdict {
    std::string emulatorId;
    RomsetFit fit{RomsetFit::Unchecked};
    /// Whether the core's listing has a set of the zip's name.
    bool listed{false};
    /// The ROMs and disks the set needs, and those of them not found.
    std::size_t required{0};
    std::vector<std::string> missing;

    bool operator==(const RomsetVerdict&) const = default;
};

/// `dat`'s verdict on set `name` (lower case) given what `zips` hold.
[[nodiscard]] RomsetVerdict matchSet(std::string_view name, const RomsetDat& dat, FolderZips& zips);

/// What a mismatching verdict reads as: "has no set armwar", "lacks 2 of 16 files: a, b".
[[nodiscard]] std::string gapOf(const RomsetVerdict& verdict, std::string_view set);

/// Marks each of the game's emulators with its verdict and picks the emulator by them: the one
/// the search picked when it matches, else the first matching one installed, else the first
/// matching one (to download). With no match and every core checked the game cannot start and
/// says so; `romset` names the matches, `romsetNearest` the nearest core's gap. Unchecked
/// verdicts change nothing.
void applyRomsets(Game& game, std::string_view set, const std::vector<RomsetVerdict>& verdicts);

/// How long a matcher waits before it refreshes a listing again.
inline constexpr std::chrono::hours romsetRetryAfter{1};

/// The text a game shows when no arcade core matches its romset.
inline constexpr std::string_view noRomsetMatch = "No arcade core matches this romset";

class RomsetMatcher {
  public:
    using Clock = RomsetDats::Clock;

    /// A matcher that is nowhere: every verdict is unchecked.
    RomsetMatcher() = default;
    explicit RomsetMatcher(RomsetDats dats);

    /// Starts a library scan at `now`. A listing is refreshed when a scan first needs it and
    /// again no sooner than `romsetRetryAfter` later, so a failing fetch is not repeated per scan.
    void begin(Clock::time_point now);

    /// Each of `cores`' verdicts on `zip`, kept from an earlier scan when nothing it was matched
    /// with has changed.
    [[nodiscard]] std::vector<RomsetVerdict> check(const std::filesystem::path& zip,
                                                   std::span<const EmulatorEntry* const> cores);

    /// Keeps the verdicts when they changed; a failure is logged.
    void save();

  private:
    struct Kept {
        FileStamp zip;
        std::map<std::string, std::uint32_t, std::less<>> stamps;
        std::vector<FileStamp> looked;
        std::vector<RomsetVerdict> verdicts;
    };

    [[nodiscard]] std::filesystem::path keptFile() const;
    void loadKept();
    /// The stamp of each of `cores`' listings, refreshed first in this scan.
    [[nodiscard]] std::map<std::string, std::uint32_t, std::less<>>
    stampsOf(std::span<const EmulatorEntry* const> cores);
    /// `entry`'s listing with the sets of `folder`'s zips; null when it cannot be had.
    [[nodiscard]] const RomsetDat* datFor(const EmulatorEntry& entry,
                                          const std::filesystem::path& folder);

    RomsetDats dats_;
    Clock::time_point now_{};
    bool keptLoaded_{false};
    bool changed_{false};
    std::map<std::string, Kept, std::less<>> kept_;
    /// When each listing was last refreshed, and its stamp then.
    struct Refresh {
        Clock::time_point tried;
        std::optional<DatStamp> stamp;
    };
    std::map<std::string, Refresh, std::less<>> refreshed_;
    std::filesystem::path loadedFolder_;
    std::map<std::string, std::optional<RomsetDat>, std::less<>> loaded_;
};

} // namespace opensu::library::roms
