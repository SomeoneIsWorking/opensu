// romset_convert — rebuilds an older romset into the zip a core's listing asks for, at launch and
// off the loop: one zip, parent and BIOS files included, so nothing need sit beside it. Every ROM
// is checked by CRC against the listing; a zip is written only whole. Kept under
// `<cache>/romsets/converted/<stamp>/<set>.zip`, the oldest going past `convertedLimit`.
#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "emulator_catalog.hpp"
#include "game.hpp"
#include "rom_tree.hpp"
#include "romset_conversions.hpp"
#include "romset_dats.hpp"

namespace opensu::library::roms {

inline constexpr std::uintmax_t convertedLimit = 2ULL * 1024 * 1024 * 1024;

class RomsetConverter {
  public:
    /// Nowhere: every build fails.
    RomsetConverter() = default;
    RomsetConverter(const std::filesystem::path& cacheDir, std::shared_ptr<const RomTree> tree,
                    RomsetConversions conversions, RomsetDats dats)
        : dir_{cacheDir / "romsets" / "converted"}, tree_{std::move(tree)},
          conversions_{std::move(conversions)}, dats_{std::move(dats)} {
    }

    /// The zip `conversion` asks for on `entry`, built unless kept; nothing, with `error`.
    [[nodiscard]] std::optional<std::filesystem::path>
    build(const RomConversion& conversion, const EmulatorEntry& entry, std::string& error) const;

  private:
    [[nodiscard]] bool write(const RomConversion& conversion, const EmulatorEntry& entry,
                             const std::filesystem::path& file, std::string& error) const;
    void trim(const std::filesystem::path& keep) const;

    std::filesystem::path dir_;
    std::shared_ptr<const RomTree> tree_;
    RomsetConversions conversions_;
    RomsetDats dats_;
};

} // namespace opensu::library::roms
