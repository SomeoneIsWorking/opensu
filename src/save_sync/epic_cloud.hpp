// epic_cloud — Epic games' saves in Epic's cloud through legendary, before a game starts and after
// it ends, with the shared rule and conflict pick, for an installed game whose metadata says it has
// cloud saves. legendary's own `sync-saves` compares times only and takes the newer side whatever
// changed, so openSU keeps its own record (`epic-saves.json`: per game the content hash of the save
// folder and the cloud save's manifest when they last synced) and moves saves only with
// `sync-saves --force-download` (the save folder backed up first under
// `<backups>/save-backups/epic/<app>/<UTC time>/`) or `--force-upload`. The cloud's newest save is
// the one a `-v sync-saves` with both transfers skipped compares with, named as its manifest
// ("2026.11.04-18.09.00.manifest", its UTC time): one request for that game's saves, where
// `list-saves` refreshes legendary's whole game list first. The save folder is the one legendary
// keeps (`info --json --offline`'s `install.save_path`), set by a `sync-saves` with both transfers
// skipped and `--accept-path` when it has none. Work runs on a thread of its own.
#pragma once

#include <condition_variable>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>

#include "library/game.hpp"
#include "save_sync.hpp"

namespace opensu::save_sync {

/// The file the record is kept in, under the state dir.
inline constexpr std::string_view epicSavesFile = "epic-saves.json";

/// The cloud save of `app` that `-v sync-saves`' output compares with, as its manifest's name;
/// empty for none, nothing when legendary did not check `app`.
[[nodiscard]] std::optional<std::string> cloudSaveIn(std::string_view output, std::string_view app);
/// When a cloud save was made, from its manifest's name; nothing when the name says no time.
[[nodiscard]] std::optional<std::int64_t> cloudSaveTime(std::string_view manifest);

class EpicCloud {
  public:
    struct Setup {
        std::string legendary{"legendary"};
        std::filesystem::path recordFile;
        /// The state dir, under whose `save-backups` a save folder is backed up before a download
        /// replaces it.
        std::filesystem::path backups;
        /// What `standing` gives changed; called on its thread.
        std::function<void()> changed;
    };

    explicit EpicCloud(Setup setup);
    ~EpicCloud();
    EpicCloud(const EpicCloud&) = delete;
    EpicCloud& operator=(const EpicCloud&) = delete;
    EpicCloud(EpicCloud&&) = delete;
    EpicCloud& operator=(EpicCloud&&) = delete;

    /// Whether `game` is an installed Epic game with cloud saves.
    [[nodiscard]] static bool serves(const library::Game& game);

    /// Syncs before `game` starts; `prepared` says when that is done.
    [[nodiscard]] std::uint64_t prepare(const library::Game& game);
    [[nodiscard]] bool prepared(std::uint64_t ticket) const;
    /// Syncs after it ended, and from "Sync now".
    void sync(const library::Game& game);
    /// Finds how its saves stand; nothing is moved.
    void look(const library::Game& game);
    /// The player's pick in a conflict: the cloud's (`theirs`) or these.
    void keep(const library::Game& game, bool theirs);
    /// How its saves stood when last asked, as the target "Epic"; nothing before that.
    [[nodiscard]] std::optional<TargetSaves> standing(const std::string& gameId) const;
    [[nodiscard]] bool busy(const std::string& gameId) const;

  private:
    enum class Pick : std::uint8_t { None, Ours, Theirs };
    struct Job {
        library::Game game;
        Pick pick{Pick::None};
        bool lookOnly{false};
        std::uint64_t ticket{0};
    };
    struct Synced {
        std::string hash;
        std::string manifest;
    };

    void queue(Job job);
    void work(const std::stop_token& stop);
    [[nodiscard]] TargetSaves run(const Job& job);
    /// legendary's output for `arguments`; nothing, with `error`, when it cannot be run or fails.
    [[nodiscard]] std::optional<std::string> legendary(const std::vector<std::string>& arguments,
                                                       std::string& error) const;
    /// The save folder legendary keeps for `app`, set first when it has none; empty, with
    /// `error`, when it can work none out.
    [[nodiscard]] std::filesystem::path saveFolder(const std::string& app,
                                                   std::string& error) const;
    /// The newest cloud save of `app`, as its manifest's name; nothing, with `error`, when
    /// legendary cannot check it.
    [[nodiscard]] std::optional<std::string> newestOf(const std::string& app,
                                                      std::string& error) const;
    void keep() const;

    Setup setup_;
    mutable std::mutex mutex_;
    std::map<std::string, Synced, std::less<>> record_;
    std::map<std::string, TargetSaves, std::less<>> standings_;
    std::deque<Job> jobs_;
    std::set<std::string, std::less<>> running_;
    std::uint64_t issued_{0};
    std::set<std::uint64_t> finished_;
    std::condition_variable_any wake_;
    /// Last, so it stops before what it uses goes.
    std::jthread worker_;
};

} // namespace opensu::save_sync
