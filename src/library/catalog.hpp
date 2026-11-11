// catalog — the library sources, built from the one configuration owner.
#pragma once

#include <filesystem>
#include <vector>

#include "config/config.hpp"
#include "epic.hpp"
#include "game.hpp"
#include "gog.hpp"
#include "own_games.hpp"
#include "roms.hpp"
#include "steam.hpp"

namespace opensu::library {

/// Which stores the catalog reads besides the ROM library and the player's own games, which it
/// always reads, and whether
/// the ROMs' emulators include standalone ones or only the built-in ones.
struct CatalogStores {
    bool steam{false};
    bool epic{false};
    bool gog{false};
    bool standaloneEmulators{false};
    /// The games of the openSUs on the LAN this one paired with.
    bool lan{false};

    bool operator==(const CatalogStores&) const = default;
};

/// The ROM folders the catalog reads: the configured ones, else those found on this machine.
[[nodiscard]] std::vector<std::filesystem::path> romRootsOf(const config::Config& config);

/// Builds the catalog's providers of `stores` from the configuration; a store left out is not
/// read at all. A `hidden` shell's ROMs start silent, no sound device and no pads, and
/// outside a Gamescope also hidden (`roms::RunnerMode`). Arcade romsets are matched with the
/// listings `romsets` keeps, which outlives the catalog. The player's own games are read from
/// `ownGames`.
[[nodiscard]] Catalog makeCatalog(const config::Config& config, bool hidden,
                                  const std::filesystem::path& ownGames, roms::RomsetLists& romsets,
                                  const CatalogStores& stores);

} // namespace opensu::library
