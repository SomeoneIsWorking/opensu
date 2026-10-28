#include "catalog.hpp"

#include <filesystem>
#include <utility>
#include <vector>

namespace opensu::library {

Catalog makeCatalog(const config::Config& config, bool hidden,
                    const std::filesystem::path& ownGames, roms::RomsetLists& romsets,
                    const CatalogStores& stores) {
    Catalog catalog;
    if (stores.steam) {
        catalog.add(std::make_unique<steam::Provider>(
            steam::Library::discover(config.home, config.steamRoots)));
    }
    if (stores.epic) {
        catalog.add(std::make_unique<epic::Provider>());
    }
    if (stores.gog) {
        catalog.add(std::make_unique<gog::Provider>(
            gog::TokenStore::under(config.dataDir),
            gog::Setup{.paths = gog::Paths::under(config.dataDir)}));
    }
    std::vector<std::filesystem::path> romRoots = config.romRoots;
    if (romRoots.empty()) {
        romRoots = roms::discoverRoots(config.home, roms::standardMountDirs(config.home));
    }
    catalog.add(std::make_unique<roms::Provider>(
        std::move(romRoots),
        roms::EmulatorSetup::standard(config, hidden, stores.standaloneEmulators),
        roms::NameDb::under(config.cacheDir), &romsets,
        roms::SwitchInfo{roms::SwitchInfo::edenKeyDirs(config.xdgDataHome, config.home),
                         config.cacheDir / "artwork" / "switch"}));
    catalog.add(std::make_unique<own::Provider>(own::Records{ownGames}));
    return catalog;
}

} // namespace opensu::library
