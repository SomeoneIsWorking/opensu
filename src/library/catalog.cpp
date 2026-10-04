#include "catalog.hpp"

#include <map>

namespace iideck::library {

/// Deliberately a small built-in set: a real ES-DE systems configuration
/// replaces it. This is a fact about emulators, not about the machine, so it
/// belongs to the library rather than to configuration.
const std::map<std::string, std::string, std::less<>>& romExtensions() {
    static const std::map<std::string, std::string, std::less<>> extensions{
        {".NES", "NES"},         {".SFC", "SNES"},           {".SMC", "SNES"},
        {".MD", "Genesis"},      {".GEN", "Genesis"},        {".GBA", "Game Boy Advance"},
        {".GB", "Game Boy"},     {".GBC", "Game Boy Color"}, {".PS1", "PlayStation"},
        {".PBP", "PlayStation"}, {".CUE", "PlayStation"},    {".CHD", "PlayStation"},
        {".ISO", "PlayStation"}, {".BIN", "PlayStation"},    {".Z64", "Nintendo 64"},
        {".N64", "Nintendo 64"}, {".SMD", "Master System"},  {".GG", "Game Gear"},
        {".MDX", "Mega Drive"},
    };
    return extensions;
}

Catalog makeCatalog(const config::Config& config) {
    Catalog catalog;
    catalog.add(std::make_unique<steam::Provider>(
        steam::Library::discover(config.home, config.steamRoots)));
    catalog.add(std::make_unique<epic::Provider>());
    catalog.add(std::make_unique<gog::Provider>(config.home));
    catalog.add(
        std::make_unique<roms::Provider>(config.romRoots, romExtensions(), config.emulators));
    return catalog;
}

} // namespace iideck::library