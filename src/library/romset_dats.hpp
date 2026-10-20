// romset_dats — the arcade cores' ROM-set listings kept under `<cache>/romsets/`, one file per
// core in `RomsetDat`'s line format behind a header naming the source's CRC-32 and when it was
// fetched. A listing older than `romsetDatLifetime` is fetched again over HTTPS from the
// catalog's sources (`RomsetSource`); a failed fetch keeps the old one.
#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

#include "emulator_catalog.hpp"
#include "net/web_client.hpp"
#include "romset_dat.hpp"

namespace opensu::library::roms {

/// How long a fetched listing serves before it is fetched again.
inline constexpr std::chrono::hours romsetDatLifetime{24 * 30};

/// What a kept listing's header says.
struct DatStamp {
    /// The CRC-32 of what it was read from, so a listing fetched again unchanged matches the same.
    std::uint32_t source{0};
    /// Seconds since the epoch.
    std::int64_t fetched{0};

    bool operator==(const DatStamp&) const = default;
};

class RomsetDats {
  public:
    using Clock = std::chrono::system_clock;

    /// Listings that are nowhere: none kept, none fetched.
    RomsetDats() = default;
    /// Kept in `dir`, fetched with `web` (null fetches nothing).
    RomsetDats(std::filesystem::path dir, const net::WebClient* web)
        : dir_{std::move(dir)}, web_{web} {
    }
    /// `<cache>/romsets`.
    [[nodiscard]] static RomsetDats under(const std::filesystem::path& cacheDir,
                                          const net::WebClient* web);

    [[nodiscard]] bool enabled() const noexcept {
        return !dir_.empty();
    }
    [[nodiscard]] const std::filesystem::path& dir() const noexcept {
        return dir_;
    }

    /// The header of `entry`'s kept listing; nothing when none is kept.
    [[nodiscard]] std::optional<DatStamp> stamp(const EmulatorEntry& entry) const;

    /// Fetches `entry`'s listing again when none is kept or the kept one is older than
    /// `romsetDatLifetime` at `now`. `error` says why a due fetch failed; false when no listing
    /// is kept afterwards.
    [[nodiscard]] bool refresh(const EmulatorEntry& entry, Clock::time_point now,
                               std::string& error) const;

    /// The kept listing of `entry`, with only the `wanted` sets' ROMs.
    [[nodiscard]] std::optional<RomsetDat>
    load(const EmulatorEntry& entry, const RomsetDat::Wanted& wanted, std::string& error) const;

    /// Keeps `dat` as `entry`'s listing, read from a source of CRC-32 `source` at `now`.
    [[nodiscard]] bool save(const EmulatorEntry& entry, const RomsetDat& dat, std::uint32_t source,
                            Clock::time_point now, std::string& error) const;

  private:
    [[nodiscard]] std::filesystem::path file(const EmulatorEntry& entry) const;
    /// The listing at `entry`'s sources and the CRC-32 of what it was read from.
    [[nodiscard]] std::optional<std::pair<RomsetDat, std::uint32_t>>
    fetch(const EmulatorEntry& entry, std::string& error) const;
    [[nodiscard]] std::optional<std::pair<RomsetDat, std::uint32_t>>
    fetchRelease(const ReleaseSource& release, std::string& error) const;

    std::filesystem::path dir_;
    const net::WebClient* web_{nullptr};
};

} // namespace opensu::library::roms
