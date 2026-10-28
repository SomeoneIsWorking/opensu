// tile — one entry in the home grid with the artwork loaded for it.
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "gpu2d/image_loader.hpp"
#include "gpu2d/texture.hpp"

#include "library/shelf.hpp"
#include "platform.hpp"
#include "vector_icon.hpp"

namespace opensu::ui {

/// One entry in the grid, with the artwork loaded for it.
struct Tile {
    library::ShelfItem item;
    /// Stable while the tile lives, so its art follows it as the shelf reorders.
    std::uint64_t id{};
    /// A folder's name, as its tile reads it.
    std::string title;
    /// A folder's game count, as its tile reads it.
    std::string caption;
    /// A launcher's logo.
    std::optional<Icon> logo;
    /// The stores a game is owned in, as icons in its corner.
    std::vector<Icon> stores;
    /// Its artwork files as the loader brings them in; empty for none.
    gpu2d::ImageHandle portrait;
    gpu2d::ImageHandle wide;
    /// Whether its files have been asked of the loader, or it has none to ask for.
    bool artRequested{false};
    /// Whether the artwork fetcher is downloading, or has queued, art for it.
    bool artDownloading{false};
    /// The tile's platform frame, or null for a game with no platform identity.
    const Platform* platform{nullptr};

    /// Whether either file is still on its way from the loader.
    [[nodiscard]] bool artLoading() const noexcept {
        return portrait.pending() || wide.pending();
    }
    /// Whether it shows the loading placeholder: no texture yet, and art downloading or loading.
    [[nodiscard]] bool awaitingArt() const noexcept {
        return art() == nullptr && (artDownloading || artLoading());
    }
    /// The art it shows once both files have landed: the portrait, else the wide image, else
    /// null.
    [[nodiscard]] const gpu2d::Texture* art() const noexcept {
        if (artLoading()) {
            return nullptr;
        }
        const gpu2d::Texture* shown = portrait.texture();
        return shown != nullptr ? shown : wide.texture();
    }
};

} // namespace opensu::ui
