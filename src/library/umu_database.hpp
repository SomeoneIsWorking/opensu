// umu_database — the umu database's ids, which pick a game's protonfixes: each store's id for a
// game (GOG's product id, Epic's app name) to its umu id (`umu-<Steam app id>`), from the listing
// the database publishes (`umu_api.php`), kept at `<data>/wine/umu-database.json` by `WineRunners`
// and read from there, so a launch never waits for it. The GAMEID a game runs under (`gameIdOf`):
// the database's id for a GOG or Epic game, `umu-<app id>` for an own game matched to a Steam app,
// else `umu-0`, which applies no fixes.
#pragma once

#include <filesystem>
#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <utility>

#include "game.hpp"

namespace opensu::library::umu {

/// The whole database as JSON: a list of {title, umu_id, codename, store, ...}.
inline constexpr std::string_view databaseApi = "https://umu.openwinecomponents.org/umu_api.php";
/// The GAMEID of a game the database does not know.
inline constexpr std::string_view unknownGameId = "umu-0";

/// Where openSU keeps the listing.
[[nodiscard]] std::filesystem::path databaseFile(const std::filesystem::path& dataDir);

class Database {
  public:
    Database() = default;

    /// The listing `text`; throws std::runtime_error when it is not one.
    [[nodiscard]] static Database parse(std::string_view text);
    /// The listing in `file`; empty when there is none yet or it cannot be read, which is logged.
    [[nodiscard]] static Database read(const std::filesystem::path& file);

    [[nodiscard]] std::size_t size() const noexcept {
        return ids_.size();
    }
    /// The GAMEID `game` runs under.
    [[nodiscard]] std::string gameIdOf(const Game& game) const;

    /// umu ids by store (`gog`, `egs`) and lowercased codename.
    using Ids = std::map<std::pair<std::string, std::string>, std::string, std::less<>>;

  private:
    explicit Database(Ids ids) : ids_{std::move(ids)} {
    }

    Ids ids_;
};

} // namespace opensu::library::umu
