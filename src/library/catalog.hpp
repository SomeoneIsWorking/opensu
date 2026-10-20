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
[[nodiscard]] Catalog makeCatalog(const config::Config& config, bool hidden);

} // namespace opensu::library