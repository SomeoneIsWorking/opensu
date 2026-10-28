// vector_icon — the shipped SVG icons, rasterised at the size they are drawn so they stay sharp
// at any resolution: the launcher logos and a console art theme's SVG cards and glyphs as white
// masks to tint, and openSU's own dock icons in their colours.
#pragma once

#include <cstdint>
#include <deque>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "gpu2d/geometry.hpp"
#include "gpu2d/image_loader.hpp"
#include "gpu2d/texture.hpp"

#include "library/game.hpp"
#include "library/sections.hpp"

namespace opensu::ui {

enum class Icon : std::uint8_t { Steam, Epic, Gog, Home, HomeSelected, Library, LibrarySelected };

/// The store's icon, or nothing for a ROM.
[[nodiscard]] std::optional<Icon> iconFor(library::Source source) noexcept;

/// A section's dock icon, as drawn while it is the active section or not.
[[nodiscard]] Icon dockIcon(library::Section section, bool selected) noexcept;

/// The icon's file under the assets folder.
[[nodiscard]] std::string_view iconFile(Icon icon) noexcept;

/// One colour of an SVG's paints drawn as another; matched on red, green and blue, and the
/// paint keeps its own opacity.
struct ColourSwap {
    gpu2d::Colour from;
    gpu2d::Colour to;
};

/// The colours `library.svg` is drawn in, for swapping: its body gradient's top and bottom, and
/// its extruded edge.
inline constexpr gpu2d::Colour libraryTop{0x48, 0xE0, 0xFF, 255};
inline constexpr gpu2d::Colour libraryBottom{0x5B, 0x4B, 0xF0, 255};
inline constexpr gpu2d::Colour libraryEdge{0x3A, 0x2F, 0xB0, 255};

/// An SVG as a `width` x `height` RGBA image, unpremultiplied: scaled to fit, centred, its paints
/// swapped by `swaps` first. Nothing when the file cannot be read or parsed.
[[nodiscard]] std::optional<std::vector<unsigned char>>
rasterise(const std::filesystem::path& svg, int width, int height,
          std::span<const ColourSwap> swaps = {});

/// An SVG as a `pixels`-square RGBA mask: white, its coverage in alpha, centred and scaled to
/// fit. Nothing when the file cannot be read or parsed.
[[nodiscard]] std::optional<std::vector<unsigned char>>
rasteriseMask(const std::filesystem::path& svg, int pixels);

/// Icon textures, one per icon, size and colouring, rasterised on the image loader's workers
/// from first use.
class IconAtlas {
  public:
    /// The icons are read from `assetsDir` and rasterised through `images`.
    IconAtlas(std::filesystem::path assetsDir, gpu2d::ImageLoader& images)
        : assetsDir_{std::move(assetsDir)}, images_{images} {
    }

    /// The icon as a white mask `pixels` square, to draw tinted; null until it lands or when it
    /// cannot be read.
    [[nodiscard]] const gpu2d::Texture* mask(Icon icon, int pixels);

    /// The SVG at `file` under the assets folder as a white mask `pixels` square; null until it
    /// lands or when it cannot be read.
    [[nodiscard]] const gpu2d::Texture* mask(std::string_view file, int pixels);

    /// The icon in its colours, with `swaps` applied, fitted and centred in `width` x `height`;
    /// null until it lands or when it cannot be read.
    [[nodiscard]] const gpu2d::Texture* image(Icon icon, int width, int height,
                                              std::span<const ColourSwap> swaps = {});

    /// Icons asked for that have not landed yet.
    [[nodiscard]] bool pending() const noexcept;
    /// Blocks until every icon asked for is decoded, for the loader's next upload; for a still
    /// frame.
    void wait();

  private:
    struct Entry {
        std::string file;
        int width{};
        int height{};
        bool mask{};
        std::vector<ColourSwap> swaps;
        gpu2d::ImageHandle image;
    };
    const gpu2d::Texture* find(std::string_view file, int width, int height, bool mask,
                               std::span<const ColourSwap> swaps);

    std::filesystem::path assetsDir_;
    gpu2d::ImageLoader& images_;
    std::deque<Entry> entries_;
};

} // namespace opensu::ui
