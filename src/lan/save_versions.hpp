// save_versions — this openSU's record of each game's saves for LAN sharing, `lan-saves.json` in
// the state dir: per game (`SaveKey`), a change counter that moves on each time the saves' content
// hash (`SaveBundle::hash`) is seen to change, and, per paired peer, the hash both held when they
// last synced. `syncStateOf` is the one rule that compares a game's saves here and on a peer
// against that last sync. Safe from any thread.
#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <mutex>
#include <string>
#include <string_view>

#include "library/game_saves.hpp"

namespace opensu::lan {

/// The file the record is kept in, under the state dir.
inline constexpr std::string_view saveVersionsFile = "lan-saves.json";

/// A game's saves on one side: how often they changed and what they hold now.
struct SaveVersion {
    std::uint64_t counter{0};
    /// The content hash; empty while there is no save.
    std::string hash;

    bool operator==(const SaveVersion&) const = default;
};

/// How a game's saves here stand against a peer's.
enum class SyncState : std::uint8_t {
    /// Both hold the same.
    InSync,
    /// Only the peer's changed since the last sync: take its.
    Pull,
    /// Only this one's changed: the peer takes it.
    Push,
    /// Both changed since the last sync, to different saves: the player picks.
    Conflict,
};

/// The rule: `ours` and `theirs` (empty for no save) against `synced`, the hash both held at the
/// last sync (empty for never). A side with no save never counts as changed against one that has.
[[nodiscard]] SyncState syncStateOf(std::string_view ours, std::string_view theirs,
                                    std::string_view synced) noexcept;

class SaveVersions {
  public:
    /// Reads `file`; a missing or unreadable one is an empty record.
    explicit SaveVersions(std::filesystem::path file);

    /// The version of `key`'s saves here now that they hash to `hash`: the counter moves on when
    /// it differs from the hash last seen. Kept.
    SaveVersion observe(const library::saves::SaveKey& key, const std::string& hash);
    /// The hash `key`'s saves had here and on `peer` when they last synced; empty for never.
    [[nodiscard]] std::string syncedWith(const library::saves::SaveKey& key,
                                         std::string_view peer) const;
    /// This side took another's saves of `key`, which hash to `hash` and were at `counter` there:
    /// its counter moves to at least that. Kept.
    void took(const library::saves::SaveKey& key, const std::string& hash, std::uint64_t counter);
    /// `key`'s saves here and on `peer` both hashed to `hash` at their last sync. Kept.
    void synced(const library::saves::SaveKey& key, const std::string& peer,
                const std::string& hash);

  private:
    struct Game {
        SaveVersion version;
        std::map<std::string, std::string, std::less<>> synced;
    };
    /// Under `mutex_`.
    void keep() const;

    std::filesystem::path file_;
    mutable std::mutex mutex_;
    /// By "system\ngame".
    std::map<std::string, Game, std::less<>> games_;
};

} // namespace opensu::lan
