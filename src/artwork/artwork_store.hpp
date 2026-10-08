// artwork_store — downloaded artwork on disk: where each game's file goes, which games still
// want one, the misses not to ask about again soon, and libretro's listings.
//
// Layout under the root: steam/<appid>.jpg, rom/<system>/<thumbnail name>.png,
// console/<system>.png, glyph/<system>.png for a console's white frame glyph, a `.miss` file
// beside any that the source does not have, libretro/<system>.txt for a listing and
// iisu/<pack> for iiSU's starter pack.
#pragma once

#include <chrono>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "library/shelf.hpp"

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

    /// The file a console's card is kept in.
    [[nodiscard]] std::filesystem::path pathFor(const library::Console& console) const;

    /// The file a system's frame glyph is kept in.
    [[nodiscard]] std::filesystem::path glyphPath(std::string_view system) const;

    /// A system's glyph file when it is stored, else empty.
    [[nodiscard]] std::filesystem::path storedGlyph(std::string_view system) const;

    /// Where the starter pack named `name` is kept.
    [[nodiscard]] std::filesystem::path packPath(std::string_view name) const;

    /// Gives every game without art of its own its downloaded file, when there is one.
    void apply(std::vector<library::Game>& games) const;
    /// The same for the consoles of a shelf; its games are left as they are.
    void apply(std::vector<library::ShelfItem>& shelf) const;

    /// Whether to ask a source for a game: it has no art, a source could supply it, and it was
    /// not missed within `missLifetime` of `now`.
    [[nodiscard]] bool wanted(const library::Game& game, Clock::time_point now) const;

    /// The same for a console: it has no card, and it was not missed within `missLifetime`.
    [[nodiscard]] bool wanted(const library::Console& console, Clock::time_point now) const;

    /// Whether to ask a source for a system's glyph: it is not stored and not missed within
    /// `missLifetime` of `now`.
    [[nodiscard]] bool wantedGlyph(std::string_view system, Clock::time_point now) const;

    /// Keeps a downloaded image, replacing the file whole. False with `error` on failure.
    bool save(const library::Game& game, std::string_view bytes, std::string& error) const;
    bool save(const library::Console& console, std::string_view bytes, std::string& error) const;
    /// Keeps a system's frame glyph.
    bool saveGlyph(std::string_view system, std::string_view bytes, std::string& error) const;
    /// Keeps the starter pack named `name`.
    bool savePack(std::string_view name, std::string_view bytes, std::string& error) const;
    /// Notes that the source has nothing for the game or console.
    void recordMiss(const library::Game& game) const;
    void recordMiss(const library::Console& console) const;
    void recordGlyphMiss(std::string_view system) const;

    /// A system's libretro listing read within `indexLifetime` of `now`, or nothing.
    [[nodiscard]] std::optional<std::vector<std::string>> index(std::string_view system,
                                                                Clock::time_point now) const;
    void saveIndex(std::string_view system, const std::vector<std::string>& names) const;

  private:
    std::filesystem::path root_;
};

} // namespace iideck::artwork
