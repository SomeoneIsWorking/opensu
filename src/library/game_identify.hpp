// game_identify — what the game file the player picked is, for Add a game's confirm card: its
// title, platform and the Steam app it matches. Clues on disk come first: a `steam_appid.txt`, a
// GOG `goggame-*.info`, a `.desktop` entry that starts the file, the folders around it, an .exe's
// version resource and the file's own name. Each is looked up in Steam's store search; the first
// whose name is one of the store's titles gives the canonical title and app id, else the best clue
// is the title.
#pragma once

#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "own_games.hpp"

namespace opensu::library::identify {

/// One app of a store search.
struct StoreApp {
    std::string appId;
    std::string title;

    bool operator==(const StoreApp&) const = default;
};

/// Searches Steam's store for `term`; empty when nothing matched or the store was not reached.
using StoreSearch = std::function<std::vector<StoreApp>(const std::string& term)>;

/// A name the files give the game, and where it came from in words for the player.
struct Clue {
    std::string name;
    std::string from;

    bool operator==(const Clue&) const = default;
};

/// What the files around a game say about it.
struct Clues {
    /// Best first.
    std::vector<Clue> names;
    /// From a `steam_appid.txt` beside the file or in a folder above it.
    std::string steamAppId;
};

/// The guess the player confirms.
struct Guess {
    std::string title;
    std::optional<own::Kind> kind;
    /// The Steam app matched; empty when none was.
    std::string steamAppId;
    /// Where the title came from, in words for the player.
    std::string from;
};

/// The ProductName, else FileDescription, in a Windows program's version resource; empty when it
/// has none or only an engine's placeholder (`BootstrapPackagedGame`, `UnrealGame`).
[[nodiscard]] std::string versionName(const std::filesystem::path& exe);

/// The file's own name as a title: separators as spaces, build suffixes (`_Steam`, `-Win64`,
/// `-Shipping`) dropped; empty for a generic launcher name (`start.sh`, `AppRun`, `game`).
[[nodiscard]] std::string fileTitle(const std::filesystem::path& file);

[[nodiscard]] Clues cluesOf(const std::filesystem::path& file);

/// What `file` is: each clue searched until one names a store app (the app of a `steam_appid.txt`
/// wins over a name), else the best clue.
[[nodiscard]] Guess identify(const std::filesystem::path& file, const StoreSearch& search);

/// The apps in Steam's `storesearch` answer.
[[nodiscard]] std::vector<StoreApp> parseStoreSearch(std::string_view json);

/// Steam's store search for `name`, its punctuation turned to spaces, which the search needs.
[[nodiscard]] std::string storeSearchUrl(std::string_view name);

} // namespace opensu::library::identify
