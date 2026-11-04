// save_sharing — the shell's shared saves (`save_sync::SaveSync`) on its loop: the folder or
// WebDAV share Settings › Saves names (`share`), the LAN's paired openSUs while LAN is on (their
// group set by `lan::LanNode`), a launch held while a game's saves are taken first (`before`), the
// game running, whose saves are not replaced, and how a game's saves stand for its details page.
#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "library/emulator_catalog.hpp"
#include "library/game.hpp"
#include "net/web_client.hpp"
#include "save_sync/save_sync.hpp"
#include "secrets/secret_store.hpp"
#include "settings/settings.hpp"

namespace opensu::app {

/// The secrets a share is opened with.
inline constexpr std::string_view sharePassphraseSecret = "save-share-passphrase";
inline constexpr std::string_view webDavPasswordSecret = "save-share-webdav-password";

class SaveSharing {
  public:
    /// `changed`: how a game's saves stand changed; called on the loop from `service`.
    SaveSharing(save_sync::SaveSync::Setup setup, secrets::SecretStore secrets,
                std::function<void()> changed);

    /// The engine the LAN adds its paired openSUs to.
    [[nodiscard]] save_sync::SaveSync& engine() noexcept {
        return *sync_;
    }

    /// Shares saves with the folder or WebDAV share `settings` names from now on, or none.
    void share(const settings::Settings& settings);
    /// The installed games and the catalog their saves are found through.
    void know(const std::vector<library::Game>& games,
              const library::roms::EmulatorCatalog& catalog);

    /// Holds `game`'s launch while its saves are synced, then calls `launch`; false, and nothing
    /// held, when no target can be asked now or a launch is held already.
    [[nodiscard]] bool before(const library::Game& game, std::function<void()> launch);
    [[nodiscard]] bool holding() const noexcept {
        return held_.has_value();
    }
    /// The game the shell runs now, or none. Every frame, main loop only.
    void playing(const library::Game* game);
    /// The game ended: its saves go to the targets.
    void ended(const library::Game& game);
    void look(const library::Game& game);
    void syncNow(const library::Game& game);
    void keep(const library::Game& game, const std::string& targetId, bool theirs);
    [[nodiscard]] std::vector<save_sync::TargetSaves> standings(const std::string& gameId) const;
    [[nodiscard]] bool syncing(const std::string& gameId) const;

    /// Lets a held launch go once its saves are synced and passes on the sync's news. Every
    /// frame, main loop only.
    void service();

  private:
    struct Playing {
        std::mutex mutex;
        std::string gameId;
    };
    struct Held {
        std::uint64_t ticket{0};
        std::function<void()> launch;
    };

    net::WebClient web_;
    secrets::SecretStore secrets_;
    std::function<void()> changed_;
    std::shared_ptr<Playing> playing_{std::make_shared<Playing>()};
    std::shared_ptr<std::atomic<bool>> news_{std::make_shared<std::atomic<bool>>(false)};
    std::optional<Held> held_;
    /// Last, so it stops before what its callbacks reach goes.
    std::unique_ptr<save_sync::SaveSync> sync_;
};

} // namespace opensu::app
