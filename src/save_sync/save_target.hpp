// save_target — a place a game's saves are shared with (`SaveSync`): a paired openSU on the LAN,
// or a folder or WebDAV share the player picked. It says how its copy of a game's saves stands,
// hands it over, and takes this one's. Each call is made on the sync's stop, which ends it.
#pragma once

#include <cstdint>
#include <optional>
#include <stop_token>
#include <string>

#include "library/game_saves.hpp"
#include "save_bundle.hpp"
#include "save_versions.hpp"

namespace opensu::save_sync {

class SaveTarget {
  public:
    /// What a target holds of a game.
    struct Held {
        enum class Kind : std::uint8_t {
            /// `status` is its copy; an empty hash for none.
            Saves,
            /// It does not have the game, so it shares no saves of it.
            NoGame,
            /// It does not share saves with this openSU.
            NotShared,
            /// It could not be asked (`problem`).
            Failed,
            /// The stop ended the asking.
            Stopped,
        };
        Kind kind{Kind::Failed};
        SaveStatus status;
        std::string problem;
    };
    /// What became of saves given to it.
    enum class Given : std::uint8_t {
        /// They were not taken (`error`).
        Failed,
        /// It was told of them and takes them itself.
        Told,
        /// It holds them now.
        Kept,
        /// The stop ended the giving.
        Stopped,
    };

    SaveTarget() = default;
    virtual ~SaveTarget() = default;
    SaveTarget(const SaveTarget&) = delete;
    SaveTarget& operator=(const SaveTarget&) = delete;
    SaveTarget(SaveTarget&&) = delete;
    SaveTarget& operator=(SaveTarget&&) = delete;

    /// Stable across runs: the record of what was synced is kept by it.
    [[nodiscard]] virtual std::string id() const = 0;
    [[nodiscard]] virtual std::string name() const = 0;
    /// Whether it can be asked now; one that cannot stands as offline.
    [[nodiscard]] virtual bool reachable() const = 0;

    [[nodiscard]] virtual Held status(const std::stop_token& stop,
                                      const library::saves::SaveKey& key) = 0;
    /// Its copy of `key`'s saves; nothing, with `error`, when they cannot be had or `stop` ended
    /// the taking.
    [[nodiscard]] virtual std::optional<SaveBundle>
    fetch(const std::stop_token& stop, const library::saves::SaveKey& key, std::string& error) = 0;
    /// This openSU's `bundle` of `key`, at `status`; `picked` when the player chose it over the
    /// target's in a conflict.
    [[nodiscard]] virtual Given give(const std::stop_token& stop,
                                     const library::saves::SaveKey& key, const SaveBundle& bundle,
                                     const SaveStatus& status, bool picked, std::string& error) = 0;
    /// Its saves of `key`, at `status`, were taken here.
    virtual void took(const std::stop_token& stop, const library::saves::SaveKey& key,
                      const SaveStatus& status) = 0;
};

} // namespace opensu::save_sync
