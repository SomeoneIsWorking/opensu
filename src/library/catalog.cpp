#include "catalog.hpp"

#include <filesystem>
#include <utility>
#include <vector>

#include "lutris_wine.hpp"

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
    roms::EmulatorSetup emulators =
        roms::EmulatorSetup::standard(config, hidden, stores.standaloneEmulators);
    roms::SwitchInfo switchInfo{roms::SwitchInfo::keyDirsOf(emulators.catalog),
                                config.cacheDir / "artwork" / "switch"};
    catalog.add(std::make_unique<roms::Provider>(std::move(romRoots), std::move(emulators),
                                                 roms::NameDb::under(config.cacheDir), &romsets,
                                                 std::move(switchInfo)));
    catalog.add(std::make_unique<own::Provider>(own::Records{ownGames},
                                                lutris::dataDir(config.xdgDataHome)));
    return catalog;
}

} // namespace opensu::library
