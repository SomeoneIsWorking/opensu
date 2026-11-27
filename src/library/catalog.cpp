#include "catalog.hpp"

#include <filesystem>
#include <utility>
#include <vector>

#include "game_folder.hpp"
#include "lutris_wine.hpp"

namespace opensu::library {

std::vector<std::filesystem::path> romRootsOf(const config::Config& config) {
    if (!config.romRoots.empty()) {
        return config.romRoots;
    }
    if (!config.findRomRoots) {
        return {};
    }
    return roms::discoverRoots(config.home, roms::standardMountDirs(config.home));
}

Catalog makeCatalog(const config::Config& config, bool hidden,
                    const std::filesystem::path& ownGames, roms::RomsetLists& romsets,
                    const CatalogStores& stores) {
    Catalog catalog;
    fumarole::Installs fumaroleInstalls =
        stores.fumarole ? fumarole::Installs::find(config.executablePath, config.fumaroleDataDir)
                        : fumarole::Installs{};
    if (stores.steam || !fumaroleInstalls.program.empty()) {
        catalog.add(std::make_unique<steam::Provider>(
            stores.steam ? steam::Library::discover(config.home, config.steamRoots)
                         : steam::Library{},
            std::move(fumaroleInstalls)));
    }
    if (stores.epic) {
        catalog.add(std::make_unique<epic::Provider>());
    }
    if (stores.gog) {
        catalog.add(std::make_unique<gog::Provider>(
            gog::TokenStore::under(config.dataDir),
            gog::Setup{.paths = gog::Paths::under(config.dataDir)}));
    }
    std::vector<std::filesystem::path> romRoots = romRootsOf(config);
    roms::EmulatorSetup emulators =
        roms::EmulatorSetup::standard(config, hidden, stores.standaloneEmulators);
    roms::SwitchInfo switchInfo{roms::SwitchInfo::keyDirsOf(emulators.catalog),
                                config.cacheDir / "artwork" / "switch"};
    catalog.add(std::make_unique<roms::Provider>(
        std::move(romRoots), std::move(emulators), roms::NameDb::under(config.cacheDir), &romsets,
        std::move(switchInfo), roms::RomsetConversions::shipped(config.assetsDir)));
    catalog.add(std::make_unique<own::Provider>(
        own::Records{ownGames}, lutris::foldersOf(config.xdgDataHome, config.xdgConfigHome),
        config.gameFolders, folder::flatpakExports(config.xdgDataHome)));
    return catalog;
}

} // namespace opensu::library
