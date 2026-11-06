// touch_art — the drawings of assets/touch at the size they are drawn on the target.
#pragma once

#include <filesystem>
#include <span>
#include <vector>

#include "svg/svg_raster.hpp"
#include "touch/touch_controls.hpp"

namespace opensu::touch {

/// Where and how strongly a drawing lands: its corner, and its opacity.
struct ArtSpot {
    int left = 0;
    int top = 0;
    float opacity = 1.0F;
};

/// `art` drawn over `pixels` (`width` by `height`, RGBA) at `spot`.
void composeOver(std::vector<unsigned char>& pixels, int width, int height, const Art& art,
                 const ArtSpot& spot);

class ArtSource {
  public:
    /// Reads from `directory` (assets/touch).
    explicit ArtSource(std::filesystem::path directory);

    /// `file` rasterised to fill `box`, its paints swapped by `swaps`; empty when it cannot be
    /// read.
    [[nodiscard]] Art svg(const char* file, const Box& box,
                          std::span<const svg::ColourSwap> swaps = {}) const;

  private:
    std::filesystem::path directory_;
};

} // namespace opensu::touch
