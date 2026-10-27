// catalog — the library sources, built from the one configuration owner.
#pragma once

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

    bool operator==(const CatalogStores&) const = default;
};

/// Builds the catalog's providers of `stores` from the configuration; a store left out is not
/// read at all. A `hidden` shell's ROMs start hidden. Arcade romsets are matched with the listings
/// `romsets` keeps, which outlives the catalog.
[[nodiscard]] Catalog makeCatalog(const config::Config& config, bool hidden,
                                  roms::RomsetLists& romsets, const CatalogStores& stores);

} // namespace opensu::library
