// artwork_store — downloaded artwork on disk: where each game's file goes, which games still
// want one, the misses not to ask about again soon, and libretro's listings.
//
// Layout under the root: steam/<appid>.jpg, rom/<system>/<thumbnail name>.png, a `.miss` file
// beside any that the source does not have, and libretro/<system>.txt for a listing.
#pragma once

#include <chrono>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "library/game.hpp"

namespace iideck::artwork {

class ArtworkStore {
  public:
    using Clock = std::filesystem::file_time_type::clock;

    /// A miss is asked about again after this long.
    static constexpr std::chrono::hours missLifetime{24 * 14};
    /// A libretro listing is read again after this long.
    static constexpr std::chrono::hours indexLifetime{24 * 30};

    explicit ArtworkStore(const std::filesystem::path& root);

    /// The file a game's downloaded art is kept in; empty for a game no source can supply.
    [[nodiscard]] std::filesystem::path pathFor(const library::Game& game) const;

    /// Gives every game without art of its own its downloaded file, when there is one.
    void apply(std::vector<library::Game>& games) const;

    /// Whether to ask a source for a game: it has no art, a source could supply it, and it was
    /// not missed within `missLifetime` of `now`.
    [[nodiscard]] bool wanted(const library::Game& game, Clock::time_point now) const;

    /// Keeps a downloaded image, replacing the file whole. False with `error` on failure.
    bool save(const library::Game& game, std::string_view bytes, std::string& error) const;
    /// Notes that the source has nothing for the game.
    void recordMiss(const library::Game& game) const;

    /// A system's libretro listing read within `indexLifetime` of `now`, or nothing.
    [[nodiscard]] std::optional<std::vector<std::string>> index(std::string_view system,
                                                                Clock::time_point now) const;
    void saveIndex(std::string_view system, const std::vector<std::string>& names) const;

  private:
    std::filesystem::path root_;
};

} // namespace iideck::artwork
