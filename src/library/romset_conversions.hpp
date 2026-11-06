// romset_conversions — how the files of an older romset make up the ROMs a core's listing asks
// for: data shipped in `romset_conversions.json` (recipes by target CRC, old zip names, ROMs a
// listing names but no core needs) and `cps2_keys.json`, written by `tools/romset_conversions.py`.
// Deleting the keys file only takes away the sets that lack a `.key`.
#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "romset_dat.hpp"

namespace opensu::library::roms {

/// A run of a ROM's bytes: every `stride`th byte of the file with CRC `source` from `offset`, or
/// `fill` when `source` is 0.
struct ConversionPiece {
    std::uint32_t source{0};
    std::uint32_t offset{0};
    std::uint32_t stride{1};
    std::uint32_t length{0};
    std::uint8_t fill{0};

    bool operator==(const ConversionPiece&) const = default;
};

/// The ten 16-bit words of a CPS2 key file.
using Cps2Key = std::array<std::uint16_t, 10>;

/// The file a `.key` is, from its words.
[[nodiscard]] std::string cps2KeyFile(const Cps2Key& key);

/// A file the zips hold, by what a listing calls it.
using HeldFiles = std::set<std::pair<std::uint32_t, std::uint64_t>>;

/// Where one ROM of a rebuilt zip comes from.
struct PlannedRom {
    const DatRom* rom{nullptr};
    /// Empty when the zips hold the file itself or `key` says what it is.
    std::vector<ConversionPiece> pieces;
    const Cps2Key* key{nullptr};
};

/// A set's ROMs from the zips: `missing` names what nothing can supply.
struct ConversionPlan {
    std::vector<PlannedRom> roms;
    std::vector<std::string> missing;
    /// Whether any ROM is built rather than copied.
    bool builds{false};
};

class RomsetConversions {
  public:
    /// No conversions.
    RomsetConversions() = default;
    /// The files shipped in `assetsDir`; what cannot be read is logged and left out.
    [[nodiscard]] static RomsetConversions shipped(const std::filesystem::path& assetsDir);
    /// Reads the two files' text; false, with `error`, when one is malformed.
    bool addConversions(std::string_view json, std::string& error);
    bool addKeys(std::string_view json, std::string& error);

    /// The sets a zip of this name may really be.
    [[nodiscard]] const std::vector<std::string>& aliasesOf(std::string_view zip) const;
    /// Every set any zip name may really be.
    [[nodiscard]] RomsetDat::Wanted aliasTargets(const RomsetDat::Wanted& zips) const;

    /// How `needs` are met from `held`. A ROM of a BIOS choice is left out when not held.
    [[nodiscard]] ConversionPlan plan(const std::vector<const DatRom*>& needs,
                                      const HeldFiles& held) const;

  private:
    using Recipe = std::vector<ConversionPiece>;

    [[nodiscard]] const Recipe* recipeFor(const DatRom& rom, const HeldFiles& held) const;

    std::multimap<std::uint32_t, Recipe> recipes_;
    std::vector<std::string> noAliases_;
    std::map<std::string, std::vector<std::string>, std::less<>> aliases_;
    std::set<std::uint32_t> optional_;
    std::map<std::uint32_t, Cps2Key> keys_;
};

/// The conversion data's file names in the assets folder.
inline constexpr std::string_view conversionsFile = "romset_conversions.json";
inline constexpr std::string_view cps2KeysFile = "cps2_keys.json";

} // namespace opensu::library::roms
