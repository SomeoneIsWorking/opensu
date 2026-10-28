// cover_overrides — the covers shown in place of a game's own: one the player chose in "Choose
// artwork", kept under the data dir as `chosen/<id>`, or a SteamGridDB cover of the tile's shape
// fitted for a cover of another shape, kept under the cache as `fitted/<id>`. The game's own file
// stays where its source put it. A choice of the game's own art is kept too (`chosen/<id>.own`),
// so no fitted cover replaces it. `<id>` is the game id percent-encoded.
#pragma once

#include <filesystem>
#include <optional>
#include <string_view>
#include <vector>

#include "artwork_store.hpp"
#include "library/game.hpp"
#include "library/shelf.hpp"

namespace opensu::artwork {

class CoverOverrides {
  public:
    /// `chosenDir` survives a cleared cache; `fittedDir` is cache.
    CoverOverrides(std::filesystem::path chosenDir, std::filesystem::path fittedDir);

    /// The cover shown for game `id` instead of its own: its chosen one, else its fitted one
    /// unless its own was chosen. Empty when it shows its own.
    [[nodiscard]] std::filesystem::path shown(std::string_view id) const;
    /// Whether the player chose the game's own cover.
    [[nodiscard]] bool keptOwn(std::string_view id) const;
    /// Whether a fitted cover should be looked for: nothing was chosen, none is kept, and none was
    /// missed within `ArtworkStore::missLifetime` of `now`.
    [[nodiscard]] bool wantsFit(std::string_view id, ArtworkStore::Clock::time_point now) const;

    /// Puts each game's shown cover in place of its own.
    void apply(std::vector<library::ShelfItem>& shelf) const;

    /// Keeps `bytes` as the player's choice for `id`.
    [[nodiscard]] bool choose(std::string_view id, std::string_view bytes,
                              std::string& error) const;
    /// Keeps the player's choice of the game's own cover.
    [[nodiscard]] bool chooseOwn(std::string_view id, std::string& error) const;
    /// Keeps `bytes` as the fitted cover of `id`.
    [[nodiscard]] bool fit(std::string_view id, std::string_view bytes, std::string& error) const;
    /// Notes that SteamGridDB has no cover of the tile's shape for `id`.
    void recordFitMiss(std::string_view id) const;

  private:
    [[nodiscard]] std::filesystem::path chosenPath(std::string_view id) const;
    [[nodiscard]] std::filesystem::path ownMarker(std::string_view id) const;
    [[nodiscard]] std::filesystem::path fittedPath(std::string_view id) const;

    std::filesystem::path chosenDir_;
    std::filesystem::path fittedDir_;
};

} // namespace opensu::artwork
