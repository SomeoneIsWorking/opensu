#include "player_files.hpp"

#include <stdexcept>
#include <string>
#include <system_error>

#include "launch/process_id.hpp"
#include "library/own_games.hpp"
#include "lucent/log.h"
#include "settings/game_settings_store.hpp"
#include "settings/mapping_store.hpp"
#include "settings/settings.hpp"

namespace opensu::app {
namespace {

namespace fs = std::filesystem;

fs::path chosenCoversUnder(const fs::path& dataDir) {
    return dataDir / "artwork" / "chosen";
}

void copyIfThere(const fs::path& from, const fs::path& to) {
    std::error_code ec;
    if (!fs::exists(from, ec)) {
        return;
    }
    fs::create_directories(to.parent_path(), ec);
    if (!ec) {
        fs::copy(from, to, fs::copy_options::recursive | fs::copy_options::overwrite_existing, ec);
    }
    if (ec) {
        throw std::runtime_error{"a hidden run cannot copy " + from.string() + " to " +
                                 to.string() + ": " + ec.message()};
    }
}

} // namespace

PlayerFiles PlayerFiles::forRun(const config::Config& config, bool hidden) {
    return PlayerFiles{
        Dirs{.config = config.configDir, .data = config.dataDir, .saves = config.savesDir},
        hidden ? config.runtimeDir / ("hidden-" + std::to_string(launch::ownProcessId()))
               : fs::path{}};
}

PlayerFiles::PlayerFiles(const Dirs& dirs, fs::path copiesDir) : copiesDir_{std::move(copiesDir)} {
    const fs::path& configDir = dirs.config;
    const fs::path& dataDir = dirs.data;
    const fs::path keptConfig = copiesDir_.empty() ? configDir : copiesDir_ / "config";
    const fs::path keptData = copiesDir_.empty() ? dataDir : copiesDir_ / "data";
    settings_ = keptConfig / settings::settingsFile;
    gameSettings_ = keptConfig / settings::gameSettingsFile;
    mappings_ = keptConfig / settings::mappingsFile;
    ownGames_ = library::own::recordsFile(keptData);
    chosenCovers_ = chosenCoversUnder(keptData);
    saves_ = copiesDir_.empty() ? dirs.saves : copiesDir_ / "saves";
    if (copiesDir_.empty()) {
        return;
    }
    std::error_code ec;
    fs::remove_all(copiesDir_, ec);
    copyIfThere(configDir / settings::settingsFile, settings_);
    copyIfThere(configDir / settings::gameSettingsFile, gameSettings_);
    copyIfThere(configDir / settings::mappingsFile, mappings_);
    copyIfThere(library::own::recordsFile(dataDir), ownGames_);
    copyIfThere(chosenCoversUnder(dataDir), chosenCovers_);
    copyIfThere(dirs.saves, saves_);
    lucent::info("settings", "hidden run: the player's files are kept in {}", copiesDir_.string());
}

config::Config PlayerFiles::savingIn(config::Config config) const {
    config.savesDir = saves_;
    return config;
}

PlayerFiles::~PlayerFiles() {
    if (copiesDir_.empty()) {
        return;
    }
    std::error_code ec;
    fs::remove_all(copiesDir_, ec);
    if (ec) {
        lucent::warn("settings", "cannot remove {}: {}", copiesDir_.string(), ec.message());
    }
}

} // namespace opensu::app
