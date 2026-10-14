// tile — one entry in the home grid with the artwork loaded for it.
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "gpu2d/texture.hpp"

#include "library/shelf.hpp"
#include "platform.hpp"
#include "vector_icon.hpp"

namespace opensu::ui {

/// One entry in the grid, with the artwork loaded for it.
struct Tile {
    library::ShelfItem item;
    /// A folder's name, as its tile reads it.
    std::string title;
    /// A folder's game count, as its tile reads it.
    std::string caption;
    /// A launcher's logo.
    std::optional<Icon> logo;
    /// The stores a game is owned in, as icons in its corner.
    std::vector<Icon> stores;
    /// Artwork loaded as textures, or nothing for none.
    std::optional<gpu2d::Texture> portrait;
    std::optional<gpu2d::Texture> wide;
    /// Whether its artwork files have been read into the textures.
    bool artLoaded{false};
    /// Whether its files are queued to decode or decoding.
    bool artRequested{false};
    /// Whether the artwork fetcher is downloading, or has queued, art for it.
    bool artDownloading{false};
    /// Changes when its files change, so a decode of the old ones is dropped.
    std::uint64_t ticket{};
    /// The tile's platform frame, or null for a game with no platform identity.
    const Platform* platform{nullptr};

    /// Whether it shows the loading placeholder: no texture yet, and art downloading or decoding.
    [[nodiscard]] bool awaitingArt() const noexcept {
        return !portrait && !wide && (artDownloading || artRequested);
    }
    /// The art it shows: the portrait, else the wide image, else null.
    [[nodiscard]] const gpu2d::Texture* art() const noexcept {
        if (portrait) {
            return &*portrait;
        }
        return wide ? &*wide : nullptr;
    }
};

} // namespace opensu::ui
