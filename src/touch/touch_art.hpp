// touch_art — the drawings of assets/touch at the size they are drawn on the target.
#pragma once

#include <filesystem>

#include "touch/touch_controls.hpp"

namespace opensu::touch {

class ArtSource {
  public:
    /// Reads from `directory` (assets/touch).
    explicit ArtSource(std::filesystem::path directory);

    /// `file` rasterised to fill `box`; empty when it cannot be read.
    [[nodiscard]] Art svg(const char* file, const Box& box) const;

  private:
    std::filesystem::path directory_;
};

} // namespace opensu::touch
