// wine — how openSU runs a Windows GOG or Epic install: the Wine runner and the prefix.
//
// Every such game shares one prefix, `<data dir>/wine-prefix`, unless the player gave it its own,
// `<data dir>/wine-prefixes/<store>-<id>`. The runner is the player's pick, else the first one
// found: the system `wine` on PATH, then the Proton builds in each Steam root's
// `compatibilitytools.d` and each Steam library's `steamapps/common`, used as their bundled Wine
// binary. Discovery only reads Steam's folders. Steam's own games are left to Steam.
#pragma once

#include <filesystem>
#include <set>
#include <string>
#include <vector>

#include "game.hpp"

namespace opensu::library::wine {

/// One Wine binary a Windows game can run under.
struct Runner {
    std::string name;
    std::filesystem::path wine;

    bool operator==(const Runner&) const = default;
};

/// Where Steam and Proton builds may be.
struct Search {
    /// The directories searched for `wine`.
    std::vector<std::filesystem::path> executablePath;
    /// Steam install roots, each with its `compatibilitytools.d`.
    std::vector<std::filesystem::path> steamRoots;
    /// Steam library folders, each with its `steamapps/common`.
    std::vector<std::filesystem::path> steamLibraries;
};

/// Every runner found, system Wine first, then compatibility tools, then Steam's Proton builds,
/// each group by name.
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

/// What a Windows launch runs with.
struct Choice {
    /// The runner the player picked; empty for the first one `found`.
    std::filesystem::path runner;
    std::vector<Runner> found;
    std::filesystem::path dataDir;
    OwnPrefixes own;
};

/// The runner `choice` names, or the first found; empty when there is none.
[[nodiscard]] std::filesystem::path runnerOf(const Choice& choice);

/// Adds `--wine <runner> --wine-prefix <prefix>` to the launch of a GOG or Epic game whose install
/// is a Windows build and makes the prefix folder; any other game is left as it is. Returns why it
/// cannot, in words for the player, else empty.
[[nodiscard]] std::string prepare(Game& game, const Choice& choice);

} // namespace opensu::library::wine
