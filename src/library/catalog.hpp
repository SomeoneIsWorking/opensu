// catalog — the library sources, built from the one configuration owner.
#pragma once

#include "config/config.hpp"
#include "epic.hpp"
#include "game.hpp"
#include "gog.hpp"
#include "roms.hpp"
#include "steam.hpp"

namespace iideck::library {

/// Builds the catalog's providers from the configuration.
[[nodiscard]] Catalog makeCatalog(const config::Config& config);

} // namespace iideck::library