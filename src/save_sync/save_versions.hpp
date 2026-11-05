// save_versions — this openSU's record of each game's shared saves, `save-sync.json` in the state
// dir: per game and profile (`SaveKey`), a change counter that moves on each time the saves'
// content hash
// (`SaveBundle::hash`) is seen to change, and, per target (`SaveTarget::id`), the hash both held
// when they last synced. `syncStateOf` is the one rule that compares a game's saves here and on a
// target against that last sync. `SaveStatus` is one side's version as targets carry it. Safe from
// any thread.
#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>

#include "library/game_saves.hpp"

namespace opensu::save_sync {

/// The file the record is kept in, under the state dir.
inline constexpr std::string_view saveVersionsFile = "save-sync.json";

/// A game's saves on one side: how often they changed and what they hold now.
struct SaveVersion {
    std::uint64_t counter{0};
    /// The content hash; empty while there is no save.
    std::string hash;

    bool operator==(const SaveVersion&) const = default;
};

/// A side's version of a game's saves with their size and newest change (Unix seconds, 0 for
/// none).
struct SaveStatus {
    std::uint64_t counter{0};
    std::string hash;
    std::uint64_t bytes{0};
    std::int64_t modified{0};

    bool operator==(const SaveStatus&) const = default;
};
/// As JSON: `counter`, `hash`, `bytes`, `modified`.
[[nodiscard]] std::string write(const SaveStatus& status);
[[nodiscard]] std::optional<SaveStatus> readSaveStatus(std::string_view text);

/// How a game's saves here stand against a target's.
enum class SyncState : std::uint8_t {
    /// Both hold the same.
    InSync,
    /// Only the target's changed since the last sync: take its.
    Pull,
    /// Only this one's changed: the target takes it.
    Push,
    /// Both changed since the last sync, to different saves: the player picks.
    Conflict,
};

/// Which side changed since the two last synced.
[[nodiscard]] SyncState stateOf(bool oursChanged, bool theirsChanged) noexcept;

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
    /// The hash `key`'s saves had here and on `target` when they last synced; empty for never.
    [[nodiscard]] std::string syncedWith(const library::saves::SaveKey& key,
                                         std::string_view target) const;
    /// This side took another's saves of `key`, which hash to `hash` and were at `counter` there:
    /// its counter moves to at least that. Kept.
    void took(const library::saves::SaveKey& key, const std::string& hash, std::uint64_t counter);
    /// `key`'s saves here and on `target` both hashed to `hash` at their last sync. Kept.
    void synced(const library::saves::SaveKey& key, const std::string& target,
                const std::string& hash);
    /// The games recorded before profiles, which name none, become `profile`'s. Kept.
    void adopt(std::string_view profile);

  private:
    struct Game {
        SaveVersion version;
        std::map<std::string, std::string, std::less<>> synced;
    };
    /// Under `mutex_`.
    void keep() const;

    std::filesystem::path file_;
    mutable std::mutex mutex_;
    /// By "profile\nsystem\ngame".
    std::map<std::string, Game, std::less<>> games_;
};

} // namespace opensu::save_sync
