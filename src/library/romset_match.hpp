// romset_match — which arcade core an arcade zip's romset is made for. A zip's central directory
// (names, sizes, CRC-32s; nothing inflated) is compared with each core's listing (`RomsetDats`):
// the set of the zip's name matches a core when every ROM it needs is in the zip, in the zips of
// its parent and BIOS, or in its devices' zips, all in the same folder, and its disks are there.
// Every file is reached through the `RomTree` that listed the folder, a picked document tree too.
// Verdicts are kept under `<cache>/romsets/matches.json` by the size and modification time of
// the zip and of every file the match looked at, so a library scan reads each zip once. A scan
// never waits on a listing: one not kept yet is asked of `RomsetLists`, and its verdict is
// pending while it downloads.
#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "emulator_catalog.hpp"
#include "game.hpp"
#include "rom_tree.hpp"
#include "romset_dat.hpp"
#include "romset_dats.hpp"
#include "romset_lists.hpp"

namespace opensu::library::roms {

/// One member of a zip as its central directory lists it.
struct ZipMember {
    std::string name;
    std::uint64_t size{0};
    std::uint32_t crc32{0};
};

/// The members of `zip`, read from its central directory only and named `label` in `error`.
/// Nothing, with `error`, when it is not a zip that can be read (zip64 included).
[[nodiscard]] std::optional<std::vector<ZipMember>>
readZipMembers(RomFile& zip, std::string_view label, std::string& error);

/// The set a file of this name holds: its stem, lower case; empty when it is no zip.
[[nodiscard]] std::string romsetOf(std::string_view fileName);

/// A file as it was when a match looked at it; `size` -1 when it was not there.
struct FileStamp {
    /// Lower case and relative to the zip's folder: `avsp.zip`, `<set>/<disk>.chd`.
    std::string path;
    std::int64_t size{-1};
    std::int64_t modified{0};

    bool operator==(const FileStamp&) const = default;
};

/// One folder of a ROM tree as a listing read it; `entries` outlives it.
class RomFolder {
  public:
    RomFolder(const RomTree& tree, std::filesystem::path location,
              std::span<const RomNode> entries);

    [[nodiscard]] const RomTree& tree() const noexcept {
        return *tree_;
    }
    [[nodiscard]] const std::filesystem::path& location() const noexcept {
        return location_;
    }
    /// The entry of lower-case `name`; null when the folder has none.
    [[nodiscard]] const RomNode* find(std::string_view name) const;
    /// The sets its zips hold.
    [[nodiscard]] RomsetDat::Wanted sets() const;

  private:
    const RomTree* tree_;
    std::filesystem::path location_;
    std::map<std::string, const RomNode*, std::less<>> entries_;
};

/// What the zips of one folder hold, each read once, and every file looked at.
class FolderZips {
  public:
    explicit FolderZips(const RomFolder& folder) : folder_{&folder} {
    }

    /// `relative` as it is now.
    [[nodiscard]] FileStamp stampOf(std::string_view relative);
    /// The members of `<set>.zip`; null when it is not there or cannot be read.
    [[nodiscard]] const std::vector<ZipMember>* members(std::string_view set);
    /// Whether `<set>/<disk>.chd` is there.
    [[nodiscard]] bool hasDisk(std::string_view set, std::string_view disk);

    [[nodiscard]] const std::vector<FileStamp>& looked() const noexcept {
        return looked_;
    }

  private:
    [[nodiscard]] const RomNode* nodeOf(std::string_view relative);

    const RomFolder* folder_;
    std::map<std::string, std::optional<std::vector<ZipMember>>, std::less<>> zips_;
    std::map<std::string, std::vector<RomNode>, std::less<>> subfolders_;
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
    /// The sets beside it whose zips the match held: parent, BIOS, devices.
    std::vector<std::string> zips;
    /// Whether the listing marks the set a BIOS: a parent to games, never a game.
    bool bios{false};

    bool operator==(const RomsetVerdict&) const = default;
};

/// `dat`'s verdict on set `name` (lower case) given what `zips` hold.
[[nodiscard]] RomsetVerdict matchSet(std::string_view name, const RomsetDat& dat, FolderZips& zips);

/// What a mismatching verdict reads as: "has no set armwar", "lacks 2 of 16 files: a, b".
[[nodiscard]] std::string gapOf(const RomsetVerdict& verdict, std::string_view set);

/// Before a game in a picked folder starts, these documents are copied beside it, for cores open
/// a parent's and BIOS's zips by path: `--beside <uri>`, which the Android launch takes out.
inline constexpr std::string_view besideArgument = "--beside";

/// Gives the launch of each of `game`'s cores `verdicts` match the zips in `folder` that it
/// leans on and no core could open by path; call before `applyRomsets`.
void addBesideZips(Game& game, const RomFolder& folder, const std::vector<RomsetVerdict>& verdicts);

/// Marks each of the game's emulators with its verdict and picks the emulator by them: the one
/// the search picked when it matches, else the first matching one installed, else the first
/// matching one (to download). With no match and every core checked the game cannot start and
/// says so; `romset` names the matches, `romsetNearest` the nearest core's gap. With no match
/// while a core's verdict is pending the game waits: `romset` is `checkingRomset` and it cannot
/// start until the listings are in. Unchecked verdicts change nothing.
void applyRomsets(Game& game, std::string_view set, const std::vector<RomsetVerdict>& verdicts);

/// The text a game shows when no arcade core matches its romset.
inline constexpr std::string_view noRomsetMatch = "No arcade core matches this romset";
/// The text a game shows while the listings it is matched with download.
inline constexpr std::string_view checkingRomset = "Checking romset…";
/// Why such a game cannot start yet.
inline constexpr std::string_view romsetListsDownloading =
    "The romset is checked once its lists download (see Downloads)";

class RomsetMatcher {
  public:
    using Clock = RomsetDats::Clock;

    /// A matcher that is nowhere: every verdict is unchecked.
    RomsetMatcher() = default;
    /// Matches with the listings `lists` keeps, asking it for those that are due.
    explicit RomsetMatcher(RomsetLists& lists);

    /// Starts a library scan at `now`.
    void begin(Clock::time_point now);

    /// Each of `cores`' verdicts on `zip` of `folder`, kept from an earlier scan when nothing it
    /// was matched with has changed. A core whose listing is not kept is pending while
    /// `RomsetLists` fetches it, else unchecked.
    [[nodiscard]] std::vector<RomsetVerdict> check(const RomFolder& folder, const RomNode& zip,
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
    /// The stamp of each of `cores`' kept listings, read once a scan; a due one is asked for.
    [[nodiscard]] std::map<std::string, std::uint32_t, std::less<>>
    stampsOf(std::span<const EmulatorEntry* const> cores);
    /// `entry`'s listing with the sets of `folder`'s zips; null when it cannot be had.
    [[nodiscard]] const RomsetDat* datFor(const EmulatorEntry& entry, const RomFolder& folder);

    RomsetLists* lists_{nullptr};
    RomsetDats dats_;
    Clock::time_point now_{};
    bool keptLoaded_{false};
    bool changed_{false};
    std::map<std::string, Kept, std::less<>> kept_;
    /// Each listing's stamp as this scan read it.
    std::map<std::string, std::optional<DatStamp>, std::less<>> stamps_;
    std::filesystem::path loadedFolder_;
    std::map<std::string, std::optional<RomsetDat>, std::less<>> loaded_;
};

} // namespace opensu::library::roms
