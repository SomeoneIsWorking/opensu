// catalog — the library sources, built from the one configuration owner.
#pragma once

#include <filesystem>
#include <map>
#include <string>
#include <vector>

#include "config/config.hpp"
#include "epic.hpp"
#include "game.hpp"
#include "gog.hpp"
#include "roms.hpp"
#include "steam.hpp"

namespace iideck::library {

/// File extension (including the dot, uppercase) to system name. The library's
/// own knowledge, not configuration: which extensions mean what is a fact about
/// emulators, and does not vary per machine.
[[nodiscard]] const std::map<std::string, std::string, std::less<>>& romExtensions();

/// Builds the catalog's providers from the configuration.
[[nodiscard]] Catalog makeCatalog(const config::Config& config);

} // namespace iideck::library