// player_files — where the shell keeps the player's own files: `settings.json`,
// `game-settings.json`, `mappings.json`, `own-games.json`, the chosen covers and the built-in
// runner's save RAM and states. An ordinary run
// uses them in place. A hidden run copies them at start into its own folder under the runtime dir
// and keeps every change there, so the run behaves as usual while the player's files stay as they
// were; the folder goes with the run.
#pragma once

#include <filesystem>

#include "config/config.hpp"

namespace opensu::app {

class PlayerFiles {
  public:
    /// openSU's config and data dirs and the runner's saves (`config::Config::savesDir`).
    struct Dirs {
        std::filesystem::path config;
        std::filesystem::path data;
        std::filesystem::path saves;
    };

    /// The files for this run: `config`'s own, or a hidden run's copies of them.
    [[nodiscard]] static PlayerFiles forRun(const config::Config& config, bool hidden);

    /// The files under `dirs`. With a `copiesDir`, each is copied there first and used from
    /// there, and the folder is removed with this; throws when they cannot be copied.
    explicit PlayerFiles(const Dirs& dirs, std::filesystem::path copiesDir = {});
    ~PlayerFiles();
    PlayerFiles(const PlayerFiles&) = delete;
    PlayerFiles& operator=(const PlayerFiles&) = delete;
    PlayerFiles(PlayerFiles&&) = delete;
    PlayerFiles& operator=(PlayerFiles&&) = delete;

    [[nodiscard]] const std::filesystem::path& settingsFile() const noexcept {
        return settings_;
    }
    [[nodiscard]] const std::filesystem::path& gameSettingsFile() const noexcept {
        return gameSettings_;
    }
    [[nodiscard]] const std::filesystem::path& mappingsFile() const noexcept {
        return mappings_;
    }
    [[nodiscard]] const std::filesystem::path& ownGamesFile() const noexcept {
        return ownGames_;
    }
    [[nodiscard]] const std::filesystem::path& chosenCoversDir() const noexcept {
        return chosenCovers_;
    }
    /// `config` with its save RAM and states where these files are.
    [[nodiscard]] config::Config savingIn(config::Config config) const;

  private:
    std::filesystem::path copiesDir_;
    std::filesystem::path settings_;
    std::filesystem::path gameSettings_;
    std::filesystem::path mappings_;
    std::filesystem::path ownGames_;
    std::filesystem::path chosenCovers_;
    std::filesystem::path saves_;
};

} // namespace opensu::app
