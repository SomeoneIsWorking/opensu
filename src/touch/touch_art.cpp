#include "touch/touch_art.hpp"

#include <algorithm>
#include <cmath>
#include <optional>
#include <utility>

#include "svg/svg_raster.hpp"

namespace opensu::touch {

ArtSource::ArtSource(std::filesystem::path directory) : directory_{std::move(directory)} {
}

Art ArtSource::svg(const char* file, const Box& box) const {
    const int width = std::max(1, static_cast<int>(std::lround(box.width)));
    const int height = std::max(1, static_cast<int>(std::lround(box.height)));
    std::optional<std::vector<unsigned char>> rgba =
        svg::rasterise(directory_ / file, width, height);
    if (!rgba) {
        return {};
    }
    return Art{std::move(*rgba), width, height};
}

} // namespace opensu::touch
