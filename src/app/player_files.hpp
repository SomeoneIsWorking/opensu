// player_files — where the shell keeps the player's own files: `settings.json`,
// `game-settings.json`, `mappings.json`, `own-games.json`, the profiles' `profiles.json`, the LAN's
// `lan.json`, the secrets its pairings and shared saves keep, its record of shared saves
// (`save-sync.json` in the state dir), the chosen covers, the built-in runner's save RAM and
// states, and the state dir save backups go to. An ordinary run uses them in place. A hidden run
// copies them at start into its own folder under the runtime dir and keeps every change there, so
// the run behaves as usual while the player's files stay as they were; the folder goes with the
// run. A hidden run's secrets stay in that folder, never the keyring.
#pragma once

#include <filesystem>
#include <string>

#include "config/config.hpp"
#include "secrets/secret_store.hpp"

namespace opensu::app {

class PlayerFiles {
  public:
    /// openSU's config, data and state dirs and the runner's saves (`config::Config::savesDir`).
    struct Dirs {
        std::filesystem::path config;
        std::filesystem::path data;
        std::filesystem::path state;
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
    /// The player profiles and which one is active.
    [[nodiscard]] const std::filesystem::path& profilesFile() const noexcept {
        return profiles_;
    }
    /// This openSU's LAN id and the peers it paired with.
    [[nodiscard]] const std::filesystem::path& lanBookFile() const noexcept {
        return lanBook_;
    }
    /// Where this run keeps what it makes of the player's saves: their backups before a copy over
    /// them, and the shared saves' staging; a hidden run's own folder.
    [[nodiscard]] const std::filesystem::path& stateDir() const noexcept {
        return state_;
    }
    /// The record of the shared saves, which follows the saves it describes.
    [[nodiscard]] const std::filesystem::path& saveVersionsFile() const noexcept {
        return saveVersions_;
    }
    /// Where the pairing and share secrets are kept: the keyring when `keyring` allows and this is
    /// not a hidden run, else owner-only files beside these.
    [[nodiscard]] secrets::SecretStore secrets(bool keyring) const {
        return secrets::SecretStore{secrets_, keyring && copiesDir_.empty()};
    }
    /// `config` with its save RAM and states where these files are.
    [[nodiscard]] config::Config savingIn(config::Config config) const;

  private:
    std::filesystem::path copiesDir_;
    // Converted at start: on Windows a path's narrow form can throw, and the destructor logs it.
    std::string copiesShown_;
    std::filesystem::path settings_;
    std::filesystem::path gameSettings_;
    std::filesystem::path mappings_;
    std::filesystem::path ownGames_;
    std::filesystem::path chosenCovers_;
    std::filesystem::path state_;
    std::filesystem::path lanBook_;
    std::filesystem::path profiles_;
    std::filesystem::path saveVersions_;
    std::filesystem::path secrets_;
    std::filesystem::path saves_;
};

} // namespace opensu::app
