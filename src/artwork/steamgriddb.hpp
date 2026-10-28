// steamgriddb — SteamGridDB's v2 API: a game's covers (grids) of a shape, found by its Steam app id
// or else by its title's first search match, with the player's API key as a bearer token. The key
// goes only to the API's own origin; images come from where the API points, without it.
#pragma once

#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "library/game.hpp"
#include "net/web_client.hpp"

namespace opensu::artwork {

/// SteamGridDB's API root.
inline constexpr std::string_view steamGridDbApi = "https://www.steamgriddb.com/api/v2";

/// The shapes SteamGridDB's covers come in.
enum class CoverShape : std::uint8_t {
    /// 2:3, Steam's library portrait.
    Portrait,
    /// Steam's store capsule, about 2.1:1.
    Landscape,
    Square,
};

/// The shape a picture of `width` by `height` is nearest.
[[nodiscard]] CoverShape coverShapeOf(int width, int height) noexcept;

/// How a game is looked up: its Steam app id when it has one, else its title.
struct SteamGridDbGame {
    std::string steamAppId;
    std::string title;
};

/// How `game` is looked up: by Steam app id for a Steam game or an own game that matched one, and
/// by title.
[[nodiscard]] SteamGridDbGame steamGridDbGameOf(const library::Game& game);

/// One cover SteamGridDB offers.
struct SteamGridDbCover {
    /// SteamGridDB's id for the picture, which a choice is kept by.
    std::int64_t id{0};
    std::string url;
    /// A smaller copy, for a picker.
    std::string thumb;
    int width{0};
    int height{0};
};

class SteamGridDb {
  public:
    explicit SteamGridDb(std::string api = std::string{steamGridDbApi});

    /// The player's key; empty disconnects. Callable from any thread.
    void setKey(std::string key);
    [[nodiscard]] bool connected() const;
    /// Whether SteamGridDB refused the key set now.
    [[nodiscard]] bool refused() const;

    /// What a lookup came to.
    struct Covers {
        enum class Status : std::uint8_t {
            /// `covers` holds what SteamGridDB has, best first; empty when it has none.
            Listed,
            /// No key is set.
            Disconnected,
            /// The key was refused (401 or 403).
            Refused,
            /// The API could not be reached; `error` says why.
            Unreachable,
        };
        Status status{Status::Listed};
        std::vector<SteamGridDbCover> covers;
        std::string error;
    };

    /// `game`'s covers of `shape`, or of every shape: by Steam app id first, then by the first
    /// title match.
    [[nodiscard]] Covers covers(const net::WebClient& web, const SteamGridDbGame& game,
                                std::optional<CoverShape> shape) const;

  private:
    /// The `data` of the API's answer to `path`, or the status it came to.
    struct Data;
    [[nodiscard]] Data call(const net::WebClient& web, const std::string& path,
                            const std::string& key) const;
    [[nodiscard]] Covers listed(const net::WebClient& web, const std::string& by,
                                std::optional<CoverShape> shape, const std::string& key) const;

    std::string api_;
    mutable std::mutex mutex_;
    std::string key_;
    mutable bool refused_{false};
};

} // namespace opensu::artwork
