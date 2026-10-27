// wine — how openSU runs a Windows GOG or Epic install: the Wine runner and the prefix.
//
// Every such game shares one prefix, `<data dir>/wine-prefix`, unless the player gave it its own,
// `<data dir>/wine-prefixes/<store>-<id>`. The runner is the player's pick, else the newest
// GE-Proton openSU installed (`ge_proton`). Runners found: openSU's GE-Proton builds, the system
// `wine` on PATH, then the Proton builds in each Steam root's `compatibilitytools.d` and each Steam
// library's `steamapps/common`. Discovery only reads Steam's folders. Steam's own games are left
// to Steam. Plain Wine runs the game itself; a Proton build runs it through umu-launcher's
// `umu-run` with `PROTONPATH`, inside the Steam Linux Runtime umu keeps under openSU's data dir.
#pragma once

#include <filesystem>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "game.hpp"

namespace opensu::library::wine {

/// One Wine or Proton build a Windows game can run under.
struct Runner {
    std::string name;
    /// Its Wine binary, which names the runner in the settings.
    std::filesystem::path wine;
    /// The Proton build's folder (the one holding `proton`); empty for plain Wine.
    std::filesystem::path proton{};
    /// Installed by openSU under its own runners folder.
    bool managed{false};

    bool operator==(const Runner&) const = default;
};

/// Where runners may be.
struct Search {
    /// The directories searched for `wine`.
    std::vector<std::filesystem::path> executablePath;
    /// Steam install roots, each with its `compatibilitytools.d`.
    std::vector<std::filesystem::path> steamRoots;
    /// Steam library folders, each with its `steamapps/common`.
    std::vector<std::filesystem::path> steamLibraries;
    /// openSU's own GE-Proton folder (`ge_proton::runnersDir`); empty for none.
    std::filesystem::path managed{};
};

/// Every runner found: openSU's GE-Proton builds newest first, system Wine, then compatibility
/// tools, then Steam's Proton builds, each group by name.
[[nodiscard]] std::vector<Runner> discover(const Search& search);

/// The games the player gave a prefix of their own, by game id.
class OwnPrefixes {
  public:
    OwnPrefixes() = default;
    explicit OwnPrefixes(std::set<std::string> ids) : ids_{std::move(ids)} {
    }

    [[nodiscard]] bool contains(const std::string& gameId) const {
        return ids_.contains(gameId);
    }
    void set(const std::string& gameId, bool own);
    [[nodiscard]] const std::set<std::string>& ids() const noexcept {
        return ids_;
    }

    bool operator==(const OwnPrefixes&) const = default;

  private:
    std::set<std::string> ids_;
};

/// The prefix every game without its own shares.
[[nodiscard]] std::filesystem::path sharedPrefix(const std::filesystem::path& dataDir);

/// `game`'s own prefix: `<dataDir>/wine-prefixes/<store>-<id>`, any character outside
/// `[A-Za-z0-9._-]` in the id made `_`.
[[nodiscard]] std::filesystem::path ownPrefix(const std::filesystem::path& dataDir,
                                              const Game& game);

/// The prefix `game` runs in.
[[nodiscard]] std::filesystem::path prefixFor(const std::filesystem::path& dataDir,
                                              const Game& game, const OwnPrefixes& own);

/// Where umu-launcher keeps the Steam Linux Runtime it fetches (`UMU_FOLDERS_PATH`), so nothing
/// lands in the player's own Steam or umu folders.
[[nodiscard]] std::filesystem::path umuFolders(const std::filesystem::path& dataDir);

/// What a Windows launch runs with.
struct Choice {
    /// The Wine binary of the runner the player picked; empty for the newest managed GE-Proton.
    std::filesystem::path runner;
    /// The games pinned to a runner of their own: its Wine binary, by game id.
    std::map<std::string, std::filesystem::path> pins{};
    std::vector<Runner> found;
    std::filesystem::path dataDir;
    OwnPrefixes own;
    /// umu-launcher's `umu-run`, which a Proton build needs; empty when none is installed.
    std::filesystem::path umuRun{};
};

/// The runner game `gameId` launches with: its pin, else the player's pick, else the newest managed
/// GE-Proton (a picked binary not found is taken as plain Wine); nothing when there is none. The
/// one rule the launch and Settings' game counts share.
[[nodiscard]] std::optional<Runner> runnerOf(const Choice& choice, const std::string& gameId);

/// The Wine binary `gameId` is set to run with: its pin, else the player's pick; empty for the
/// newest managed GE-Proton.
[[nodiscard]] std::filesystem::path pickOf(const Choice& choice, const std::string& gameId);

/// How a program runs under a runner in a prefix.
struct Invocation {
    /// What goes before the Windows program: `umu-run`, or the Wine binary.
    std::vector<std::string> command;
    /// `NAME=value` pairs the program's environment gets.
    std::vector<std::string> environment;
};

/// The invocation of `runner` in `prefix`: plain Wine with `WINEPREFIX`; a Proton build through
/// `umuRun` with `PROTONPATH`, `WINEPREFIX`, `GAMEID`, `STORE` (`store`, may be empty) and
/// `UMU_FOLDERS_PATH` under `dataDir`.
[[nodiscard]] Invocation invocationOf(const Runner& runner, const std::filesystem::path& prefix,
                                      const std::filesystem::path& umuRun,
                                      const std::filesystem::path& dataDir, std::string_view store);

/// Where the saves of a prefix plain Wine made are copied before a Proton build first runs in it.
[[nodiscard]] std::filesystem::path wineSavesCopy(const std::filesystem::path& prefix);

/// Before a Proton build first runs in `prefix`: when plain Wine made it (registry files, no
/// Proton `pfx` link), copies its `drive_c/users`, `drive_c/ProgramData` and registry files to
/// `wineSavesCopy(prefix)` once. Proton then updates the prefix in place and umu links its
/// `steamuser` folder to the player's, so the saves stay where the games look. Why it could not,
/// else empty.
[[nodiscard]] std::string keepWineSaves(const std::filesystem::path& prefix);

/// A launch prepared for its runner.
struct Prepared {
    /// Why it cannot run, in words for the player; empty when it can.
    std::string refusal;
    /// What the launch's environment gets.
    std::vector<std::string> environment;
};

/// Readies the launch of a GOG or Epic game whose install is a Windows build and makes its prefix:
/// `--wine <binary> --wine-prefix <prefix>` for plain Wine, `--no-wine --wrapper <umu-run>` and
/// umu's environment for a Proton build. Any other game is left as it is.
[[nodiscard]] Prepared prepare(Game& game, const Choice& choice);

} // namespace opensu::library::wine
