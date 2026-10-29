// svg_raster — an SVG file rasterised to RGBA at the size it is drawn, through nanosvg: the shell's
// icons (`ui/vector_icon`) and the runner's touch controls.
#pragma once

#include <filesystem>
#include <optional>
#include <span>
#include <vector>

#include "gpu2d/geometry.hpp"

namespace opensu::svg {

/// One colour of an SVG's paints drawn as another; matched on red, green and blue, and the
/// paint keeps its own opacity.
struct ColourSwap {
    gpu2d::Colour from;
    gpu2d::Colour to;
};

/// `colour`'s red, green and blue packed as nanosvg packs them, red in the low byte.
[[nodiscard]] unsigned int packedRgb(const gpu2d::Colour& colour) noexcept;

/// An SVG as a `width` x `height` RGBA image, unpremultiplied: scaled to fit, centred, its paints
/// swapped by `swaps` first. Nothing when the file cannot be read or parsed.
[[nodiscard]] std::optional<std::vector<unsigned char>>
rasterise(const std::filesystem::path& svg, int width, int height,
          std::span<const ColourSwap> swaps = {});

/// An SVG as a `pixels`-square RGBA mask: white, its coverage in alpha, centred and scaled to
/// fit. Nothing when the file cannot be read or parsed.
[[nodiscard]] std::optional<std::vector<unsigned char>>
rasteriseMask(const std::filesystem::path& svg, int pixels);

} // namespace opensu::svg
