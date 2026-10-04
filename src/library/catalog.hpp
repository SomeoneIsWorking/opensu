// catalog — the configuration the app runs with, and the providers built from
// it.
#pragma once

#include <filesystem>
#include <map>
#include <string>
#include <vector>

#include "epic.hpp"
#include "game.hpp"
#include "gog.hpp"
#include "roms.hpp"
#include "steam.hpp"

namespace iideck::library {

/// Reads the process environment once into a typed configuration.
struct Config {
    /// Explicit Steam install roots; discovered when empty.
    std::vector<std::filesystem::path> steamRoots;
    /// Directories scanned for emulator ROMs.
    std::vector<std::filesystem::path> romRoots;
    /// Emulator commands keyed by uppercase system name.
    std::map<std::string, std::vector<std::string>, std::less<>> emulators;
    /// File extension (including the dot, uppercase) to system name.
    std::map<std::string, std::string, std::less<>> extensions;
};

/// Reads IIDECK_* from the environment. This is the only place the environment
/// is read.
[[nodiscard]] Config readConfig();

/// Builds the catalog's providers from a configuration.
[[nodiscard]] Catalog makeCatalog(const Config& config);

} // namespace iideck::library