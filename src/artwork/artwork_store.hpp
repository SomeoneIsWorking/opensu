// artwork_store — downloaded artwork on disk: where each game's file goes, which games still
// want one, the misses not to ask about again soon, and libretro's listings.
//
// Layout under the root: steam/<appid>.jpg, rom/<system>/<thumbnail name>.png, and for iiSU's
// console art console/<system>.png (a card), glyph/<system>.png (a frame glyph) and
// iisu/<pack> (the starter pack); a `.miss` file beside any that the source does not have,
// libretro/<system>.txt for a listing and sound/<file> for an APK asset, a UI sound. switch/ is
// library::roms::SwitchInfo's, a Switch game's own title and icon.
#pragma once

#include <chrono>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "iisu_assets.hpp"
#include "library/shelf.hpp"

namespace opensu::artwork {

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

    /// The file a system's iiSU card is kept in.
    [[nodiscard]] std::filesystem::path cardPath(std::string_view system) const;
    /// The file a system's iiSU frame glyph is kept in.
    [[nodiscard]] std::filesystem::path glyphPath(std::string_view system) const;
    /// A system's iiSU card or glyph file when it is stored, else empty.
    [[nodiscard]] std::filesystem::path storedCard(std::string_view system) const;
    [[nodiscard]] std::filesystem::path storedGlyph(std::string_view system) const;
    /// Where the starter pack named `name` is kept.
    [[nodiscard]] std::filesystem::path packPath(std::string_view name) const;

    /// The file an APK asset (a UI sound) is kept in.
    [[nodiscard]] std::filesystem::path assetPath(const ApkAsset& asset) const;

    /// An asset's file when it is stored, else empty.
    [[nodiscard]] std::filesystem::path storedAsset(const ApkAsset& asset) const;

    /// Gives every game without art of its own its downloaded file, when there is one.
    void apply(std::vector<library::Game>& games) const;
    /// The same for the games of a shelf, which holds copies made before a later download.
    void apply(std::vector<library::ShelfItem>& shelf) const;

    /// Whether to ask a source for a game: it has no art, a source could supply it, and it was
    /// not missed within `missLifetime` of `now`.
    [[nodiscard]] bool wanted(const library::Game& game, Clock::time_point now) const;

    /// Whether to ask iiSU's APK for a system's card or glyph: it is not stored and not missed
    /// within `missLifetime` of `now`.
    [[nodiscard]] bool wantedCard(std::string_view system, Clock::time_point now) const;
    [[nodiscard]] bool wantedGlyph(std::string_view system, Clock::time_point now) const;
    /// Keeps a system's iiSU card or glyph, or notes that the APK has none.
    bool saveCard(std::string_view system, std::string_view bytes, std::string& error) const;
    bool saveGlyph(std::string_view system, std::string_view bytes, std::string& error) const;
    void recordCardMiss(std::string_view system) const;
    void recordGlyphMiss(std::string_view system) const;
    /// Keeps the starter pack named `name`.
    bool savePack(std::string_view name, std::string_view bytes, std::string& error) const;

    /// Keeps a downloaded image, replacing the file whole. False with `error` on failure.
    bool save(const library::Game& game, std::string_view bytes, std::string& error) const;
    /// Whether to ask the APK for an asset: it is not stored and not missed within `missLifetime`
    /// of `now`.
    [[nodiscard]] bool wantedAsset(const ApkAsset& asset, Clock::time_point now) const;

    /// Keeps an asset as the APK holds it.
    bool saveAsset(const ApkAsset& asset, std::string_view bytes, std::string& error) const;
    void recordAssetMiss(const ApkAsset& asset) const;

    /// Notes that the source has nothing for the game.
    void recordMiss(const library::Game& game) const;

    /// A system's libretro listing read within `indexLifetime` of `now`, or nothing.
    [[nodiscard]] std::optional<std::vector<std::string>> index(std::string_view system,
                                                                Clock::time_point now) const;
    void saveIndex(std::string_view system, const std::vector<std::string>& names) const;

  private:
    /// Gives a game without art of its own its downloaded file, when there is one.
    void giveStored(library::Game& game) const;

    std::filesystem::path root_;
};

} // namespace opensu::artwork
