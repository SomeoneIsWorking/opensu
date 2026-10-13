// png_encoder — RGBA pixels as PNG bytes, through libpng.
#pragma once

#include <cstdint>
#include <span>
#include <string>

namespace opensu::retro {

/// Rows of `width` RGBA pixels, top first, as a PNG file's bytes. Empty when libpng fails.
[[nodiscard]] std::string encodePng(std::span<const std::uint8_t> rgba, unsigned width,
                                    unsigned height);

} // namespace opensu::retro
