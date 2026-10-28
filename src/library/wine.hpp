// wine — how openSU runs a Windows GOG or Epic install, or a Windows `.exe` the player added: the
// Wine runner and the prefix.
//
// Every store game shares one prefix, `<data dir>/wine-prefix`, unless the player gave it its own,
// `<data dir>/wine-prefixes/<store>-<id>`; an added `.exe` always has its own (`own-<id>`). The
// runner is the game's pin, else the player's pick, else Automatic, the first that can run of: the
// newest GE-Proton openSU installed (`ge_proton`), the newest Proton in Steam's folders, what
// Lutris uses (`lutris`), system Wine. A launch never waits for a download. Discovery only reads
// Steam's and Lutris's folders. Steam's own games are left to Steam. Plain Wine runs the game
// itself; a Proton build runs it through `umu-run` (openSU's, else one on PATH, else Lutris's) with
// `PROTONPATH`, inside the Steam Linux Runtime umu keeps under openSU's data dir.
#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "game.hpp"
#include "umu_database.hpp"

namespace opensu::library::wine {

/// A runner's name as the player reads it, from its folder or Lutris name: the CPU suffix
/// (`-x86_64`, `-aarch64`) and parentheses dropped, a space before the version
/// ("GE-Proton11-5-x86_64" is "GE-Proton 11-5", "Proton 9.0 (Beta)" is "Proton 9.0 Beta").
[[nodiscard]] std::string displayName(std::string_view name);

/// Who put a runner on this machine.
enum class Origin : std::uint8_t {
    /// openSU's own GE-Proton builds.
    OpenSu,
    /// Added to Steam's `compatibilitytools.d` (GE-Proton and other third-party builds), by
    /// whatever put them there.
    SteamTools,
    /// Valve's Proton builds Steam installed into a library's `steamapps/common`.
    Steam,
    /// Lutris's `runners/wine`.
    Lutris,
    System,
};

/// One Wine or Proton build a Windows game can run under.
struct Runner {
    std::string name;
    /// Its Wine binary, which names the runner in the settings.
    std::filesystem::path wine;
    /// The Proton build's folder (the one holding `proton`); empty for plain Wine.
    std::filesystem::path proton{};
    Origin origin{Origin::System};

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
    /// Lutris's data folder (`lutris::dataDir`); empty for none.
    std::filesystem::path lutris{};
};

/// Every runner found in Automatic's order: openSU's GE-Proton builds, the Proton builds in Steam's
/// `compatibilitytools.d` and `steamapps/common`, Lutris's Wine builds, each newest first, then
/// system Wine.
[[nodiscard]] std::vector<Runner> discover(const Search& search);

/// The `umu-run` a Proton build runs through: openSU's `managed` one, else `umu-run` on PATH, else
/// Lutris's; empty when there is none.
[[nodiscard]] std::filesystem::path umuRunOf(const std::filesystem::path& managed,
                                             const Search& search);

/// The runner Lutris uses for a game with no version of its own, among `found`: the newest
/// GE-Proton in a `compatibilitytools.d` for Lutris's "ge-proton", a build by name, or the system
/// Wine it names; nothing when it is not on this machine (Lutris would have umu download it).
[[nodiscard]] std::optional<Runner> lutrisDefault(const std::filesystem::path& lutrisData,
                                                  const std::vector<Runner>& found);

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

/// `game`'s own prefix: `<dataDir>/wine-prefixes/<key(store)>-<id>`, any character outside
/// `[A-Za-z0-9._-]` in the id made `_`.
[[nodiscard]] std::filesystem::path ownPrefix(const std::filesystem::path& dataDir,
                                              const Game& game);

/// The prefix `game` runs in: its own for a game in `own` or one the player added.
[[nodiscard]] std::filesystem::path prefixFor(const std::filesystem::path& dataDir,
                                              const Game& game, const OwnPrefixes& own);

/// Where umu-launcher keeps the Steam Linux Runtime it fetches (`UMU_FOLDERS_PATH`), so nothing
/// lands in the player's own Steam or umu folders.
[[nodiscard]] std::filesystem::path umuFolders(const std::filesystem::path& dataDir);

/// What a Windows launch runs with.
struct Choice {
    /// The Wine binary of the runner the player picked; empty for Automatic.
    std::filesystem::path runner;
    /// The games pinned to a runner of their own: its Wine binary, by game id.
    std::map<std::string, std::filesystem::path> pins{};
    /// In Automatic's order (`discover`).
    std::vector<Runner> found;
    /// What Lutris uses (`lutrisDefault`).
    std::optional<Runner> lutris{};
    std::filesystem::path dataDir;
    OwnPrefixes own;
    /// The `umu-run` a Proton build needs (`umuRunOf`); empty when there is none.
    std::filesystem::path umuRun{};
    /// The umu database a Proton launch takes its GAMEID from; null runs every game as `umu-0`.
    const umu::Database* umu{nullptr};
};

/// Why a game runs under its runner.
enum class Reason : std::uint8_t {
    Pinned,
    Picked,
    OpenSuGeProton,
    SteamProton,
    Lutris,
    SystemWine
};

struct Resolved {
    Runner runner;
    Reason reason{Reason::Picked};
};

/// The runner game `gameId` launches with and why: its pin, else the player's pick (a binary not
/// found is taken as plain Wine), else Automatic's first that can run: openSU's newest GE-Proton,
/// Steam's newest Proton, Lutris's, system Wine, a Proton build only with a `umu-run`. Nothing when
/// none can. The one rule the launch, the test and Settings' game counts share.
[[nodiscard]] std::optional<Resolved> resolve(const Choice& choice, const std::string& gameId);

/// `resolve`'s runner.
[[nodiscard]] std::optional<Runner> runnerOf(const Choice& choice, const std::string& gameId);

/// The Wine binary `gameId` is set to run with: its pin, else the player's pick; empty for
/// Automatic.
[[nodiscard]] std::filesystem::path pickOf(const Choice& choice, const std::string& gameId);

/// How a program runs under a runner in a prefix.
struct Invocation {
    /// What goes before the Windows program: `umu-run`, or the Wine binary.
    std::vector<std::string> command;
    /// `NAME=value` pairs the program's environment gets.
    std::vector<std::string> environment;
};

/// The invocation of `runner` in `prefix`: plain Wine with `WINEPREFIX`; a Proton build through
/// `umuRun` with `PROTONPATH`, `WINEPREFIX`, `GAMEID` (`gameId`, which picks the protonfixes),
/// `STORE` (`store`, may be empty) and `UMU_FOLDERS_PATH` under `dataDir`.
[[nodiscard]] Invocation invocationOf(const Runner& runner, const std::filesystem::path& prefix,
                                      const std::filesystem::path& umuRun,
                                      const std::filesystem::path& dataDir, std::string_view store,
                                      std::string_view gameId);

/// Where the saves of a prefix plain Wine made are copied before a Proton build first runs in it.
[[nodiscard]] std::filesystem::path wineSavesCopy(const std::filesystem::path& prefix);

/// Where plain Wine runs for a prefix Proton made: `<prefix>.plain-wine`.
[[nodiscard]] std::filesystem::path plainWinePrefix(const std::filesystem::path& prefix);

/// The prefix a runner runs in, readied; `refusal` says why it could not be.
struct ReadyPrefix {
    std::filesystem::path prefix;
    std::string refusal;
};

/// Readies `prefix` (a game's or the test's) for `runner`. Proton upgrades a plain-Wine prefix in
/// place and umu links its `steamuser` folder to the player's, so the saves stay where the games
/// look; the first time, `drive_c/users`, `drive_c/ProgramData` and the registry files are copied
/// to `wineSavesCopy` as well. Plain Wine never runs in a prefix Proton made (umu's `pfx` link):
/// Proton links its build's DLLs into `system32`, which plain Wine can neither load once that
/// build is gone nor replace (`wineboot` fails with "file exists"), so it runs in
/// `plainWinePrefix`, which starts from a copy of the Proton prefix's `drive_c/users` and
/// `drive_c/ProgramData`.
[[nodiscard]] ReadyPrefix readyPrefix(const std::filesystem::path& prefix, const Runner& runner);

/// A launch prepared for its runner.
struct Prepared {
    /// Why it cannot run, in words for the player; empty when it can.
    std::string refusal;
    /// What the launch's environment gets.
    std::vector<std::string> environment;
};

/// Readies the launch of a GOG or Epic game whose install is a Windows build and makes its prefix:
/// `--wine <binary> --wine-prefix <prefix>` for plain Wine, `--no-wine --wrapper <umu-run>` and
/// umu's environment for a Proton build. An added `.exe` is started by the runner itself
/// (`invocationOf`). Any other game is left as it is.
[[nodiscard]] Prepared prepare(Game& game, const Choice& choice);

} // namespace opensu::library::wine
