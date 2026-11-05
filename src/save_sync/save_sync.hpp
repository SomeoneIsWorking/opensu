// save_sync — the saves of the games installed here shared with each target (`SaveTarget`) the
// player shares them with: paired openSUs on the LAN, a folder or a WebDAV share. A game's saves
// are those of the emulator it starts with (`library::saves::launchedSaves`) kept for one player
// profile, matched across targets by `SaveKey` (console, game and profile); each job names the
// profile it syncs; each side's version is in `SaveVersions`, and `syncStateOf` decides. A
// newer copy there is fetched into a staging folder and copied over the saves here by
// `library::saves::SaveCopier`, which backs them up first, and refused while the emulator runs
// (`Setup::inUse`). A newer copy here is given to the target. A save changed on both sides is left
// alone until the player picks one (`keep`).
//
// Targets come in groups (`setTargets`), each listed afresh for every sync. The shell asks for a
// sync before a game starts (`prepare`, which it waits for), after it ends and from "Sync now"
// (`sync`), and how its saves stand when its details open (`look`); a target may ask too
// (`heard`, `syncedWith`, `statusOf`, `sendable`). The work runs on one thread of its own, one
// game at a time, and never holds a lock the asking threads take while it waits on a target.
#pragma once

#include <condition_variable>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "library/emulator_catalog.hpp"
#include "library/game.hpp"
#include "library/game_saves.hpp"
#include "library/save_copy.hpp"
#include "save_bundle.hpp"
#include "save_target.hpp"
#include "save_versions.hpp"

namespace opensu::save_sync {

/// How a game's saves here stand with one target, as the details page shows them.
struct TargetSaves {
    enum class Standing : std::uint8_t {
        InSync,
        /// The target's are newer and are taken here before the game starts.
        NewerThere,
        /// These are newer; the target takes them.
        NewerHere,
        /// Both changed since they last synced: the player picks.
        Conflict,
        /// It cannot be asked now.
        Offline,
        /// It does not share saves with this openSU.
        NotShared,
        /// It could not be asked, or its saves could not be taken (`problem`).
        Failed,
    };
    std::string targetId;
    std::string targetName;
    Standing standing{Standing::Offline};
    SaveStatus ours;
    /// While it is reachable and answered.
    std::optional<SaveStatus> theirs;
    std::string problem;

    bool operator==(const TargetSaves&) const = default;
};

class SaveSync {
  public:
    struct Setup {
        std::filesystem::path versionsFile;
        /// `SaveCopier`'s root: its backups go under it.
        std::filesystem::path backups;
        /// Where a target's saves wait before they are copied in.
        std::filesystem::path staging;
        /// The built-in runner's saves (`Config::savesDir`).
        std::filesystem::path runnerSaves;
        /// Why `saves` of `game` cannot be replaced now ("Close Eden first: it is running"); empty
        /// when they can. Called on the sync's thread.
        std::function<std::string(const library::Game& game,
                                  const library::saves::EmulatorSaves& saves)>
            inUse;
        /// What `standings` gives changed; called on the sync's thread.
        std::function<void()> changed;
        /// `profile`'s place in the order of profiles, which picks a Switch emulator's user
        /// (`ProfileBook::seatOf`); nothing when it is not here. Called on the sync's thread.
        std::function<std::optional<std::size_t>(const std::string& profile)> seatOf;
    };
    /// The targets of a group as they are now.
    using Targets = std::function<std::vector<std::shared_ptr<SaveTarget>>()>;

    explicit SaveSync(Setup setup);
    ~SaveSync();
    SaveSync(const SaveSync&) = delete;
    SaveSync& operator=(const SaveSync&) = delete;
    SaveSync(SaveSync&&) = delete;
    SaveSync& operator=(SaveSync&&) = delete;

    /// Ends the work under way and takes no more; targets may still ask.
    void stop();

    /// The games installed here and the catalog their emulators' saves are found through.
    void know(const std::vector<library::Game>& games, library::roms::EmulatorCatalog catalog);
    /// `targets` lists `group`'s targets from now on; an empty one drops the group. Returns once
    /// no sync uses the group's earlier targets.
    void setTargets(std::string_view group, Targets targets);

    // A target asks, on its own threads.
    /// The version of `key`'s saves here; nothing when no game here has that key.
    [[nodiscard]] std::optional<SaveStatus> statusOf(const library::saves::SaveKey& key);
    /// `key`'s saves here as sent (`write(SaveBundle)`); nothing, with `error`, when there is no
    /// such game or they cannot be read.
    [[nodiscard]] std::optional<std::string> sendable(const library::saves::SaveKey& key,
                                                      std::string& error);
    /// `targetId` has newer saves of `key`'s game, or the player there picked its saves over
    /// these (`picked`).
    void heard(const std::string& targetId, const library::saves::SaveKey& key, bool picked);
    /// `targetId` now holds `hash` of `key`'s game, taken from here.
    void syncedWith(const std::string& targetId, const library::saves::SaveKey& key,
                    const std::string& hash);

    // The shell asks, each for the saves of `profile`.
    /// Syncs `game` with every target before it starts; `prepared` says when that is done.
    [[nodiscard]] std::uint64_t prepare(const library::Game& game, const std::string& profile);
    [[nodiscard]] bool prepared(std::uint64_t ticket) const;
    /// Syncs `game` with every target: after it ended, and from "Sync now".
    void sync(const library::Game& game, const std::string& profile);
    /// Syncs every game known with every target: a profile just linked takes its saves.
    void syncAll(const std::string& profile);
    /// Finds how `game`'s saves stand with each target, for `standings`.
    void look(const library::Game& game, const std::string& profile);
    /// Whether `game` has saves to share and a target can be asked now.
    [[nodiscard]] bool sharing(const library::Game& game) const;
    /// The player's pick for `game`'s conflict with `targetId`: its saves (`theirs`) or these.
    void keep(const library::Game& game, const std::string& profile, const std::string& targetId,
              bool theirs);
    /// How `profile`'s saves of `gameId` stood with each target when last looked; empty before
    /// that or when there is no target.
    [[nodiscard]] std::vector<TargetSaves> standings(const std::string& gameId,
                                                     const std::string& profile) const;
    /// Whether a sync of `gameId` is waiting or under way.
    [[nodiscard]] bool busy(const std::string& gameId) const;

  private:
    /// Whose saves the player picked in a conflict.
    enum class Pick : std::uint8_t { None, Ours, Theirs };
    struct Job {
        library::Game game;
        std::string profile;
        /// Only this target; all when empty.
        std::string targetId;
        Pick pick{Pick::None};
        /// Only how the saves stand; nothing is taken or given.
        bool lookOnly{false};
        std::uint64_t ticket{0};
    };
    /// The game's saves here: which emulator's, and their bundle.
    struct Local {
        library::saves::SaveKey key;
        std::vector<library::saves::EmulatorSaves> all;
        library::saves::EmulatorSaves launched;
        SaveBundle bundle;
        SaveStatus status;
    };

    void queue(Job job);
    void work(const std::stop_token& stop);
    void run(const std::stop_token& stop, const Job& job);
    /// Every group's targets as they are now.
    [[nodiscard]] std::vector<std::shared_ptr<SaveTarget>> targets() const;
    /// One target's standing, after taking or giving what `job` allows; nothing when it does not
    /// have the game or `stop` ended it. Off the work lock.
    [[nodiscard]] std::optional<TargetSaves> syncWith(const std::stop_token& stop, const Job& job,
                                                      SaveTarget& target);
    /// `profile`'s saves of `game` here, read under the work lock; nothing, with `error`, when it
    /// has none to sync.
    [[nodiscard]] std::optional<Local> localOf(const library::Game& game,
                                               const std::string& profile, std::string& error);
    /// Takes `bundle` from `target` over the saves here. Empty when done, else why not.
    [[nodiscard]] std::string take(const Job& job, const SaveTarget& target,
                                   const SaveBundle& bundle, const SaveStatus& theirs,
                                   const std::string& expected);
    [[nodiscard]] std::optional<library::Game> gameOf(const library::saves::SaveKey& key) const;

    Setup setup_;
    SaveVersions versions_;
    library::saves::SaveCopier copier_;

    mutable std::mutex mutex_;
    std::vector<library::Game> games_;
    library::roms::EmulatorCatalog catalog_;
    std::map<std::string, Targets, std::less<>> groups_;
    /// By "profile\ngame id".
    std::map<std::string, std::vector<TargetSaves>, std::less<>> standings_;
    std::deque<Job> jobs_;
    std::set<std::string, std::less<>> running_;
    std::uint64_t issued_{0};
    std::set<std::uint64_t> finished_;
    std::condition_variable_any wake_;
    /// Held while the saves here are read or replaced.
    std::mutex work_;
    /// Held while a job runs, so a group's targets can be dropped between jobs.
    std::mutex job_;
    /// Last, so it stops before what it uses goes.
    std::jthread worker_;
};

} // namespace opensu::save_sync
