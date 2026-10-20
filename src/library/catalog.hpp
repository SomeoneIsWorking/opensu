// catalog — the library sources, built from the one configuration owner.
#pragma once

#include "config/config.hpp"
#include "epic.hpp"
#include "game.hpp"
#include "gog.hpp"
#include "roms.hpp"
#include "steam.hpp"

namespace opensu::library {

/// Builds the catalog's providers from the configuration; a `hidden` shell's ROMs start hidden.
/// Arcade romsets are matched with the listings `romsets` keeps, which outlives the catalog.
[[nodiscard]] Catalog makeCatalog(const config::Config& config, bool hidden,
                                  roms::RomsetLists& romsets);

} // namespace opensu::library