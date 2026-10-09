#include "catalog.hpp"

#include <filesystem>
#include <utility>
#include <vector>

namespace opensu::library {

Catalog makeCatalog(const config::Config& config) {
    Catalog catalog;
    catalog.add(std::make_unique<steam::Provider>(
        steam::Library::discover(config.home, config.steamRoots)));
    catalog.add(std::make_unique<epic::Provider>());
    catalog.add(
        std::make_unique<gog::Provider>(gog::TokenStore::under(config.dataDir),
                                        gog::Setup{.paths = gog::Paths::under(config.dataDir)}));
    std::vector<std::filesystem::path> romRoots = config.romRoots;
    if (romRoots.empty()) {
        romRoots = roms::discoverRoots(config.home, roms::standardMountDirs(config.home));
    }
    catalog.add(std::make_unique<roms::Provider>(
        std::move(romRoots),
        roms::Emulators::discover(
            roms::EmulatorSearch::standard(config.home, config.executablePath), config.emulators)));
    return catalog;
}

} // namespace opensu::library