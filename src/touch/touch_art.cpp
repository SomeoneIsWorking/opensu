#include "touch/touch_art.hpp"

#include <algorithm>
#include <cmath>
#include <optional>
#include <utility>

namespace opensu::touch {

void composeOver(std::vector<unsigned char>& pixels, int width, int height, const Art& art,
                 const ArtSpot& spot) {
    for (int row = 0; row < art.height; ++row) {
        const int y = spot.top + row;
        if (y < 0 || y >= height) {
            continue;
        }
        for (int column = 0; column < art.width; ++column) {
            const int x = spot.left + column;
            if (x < 0 || x >= width) {
                continue;
            }
            const unsigned char* source =
                &art.rgba[((static_cast<std::size_t>(row) * art.width) + column) * 4];
            const float above = static_cast<float>(source[3]) / 255.0F * spot.opacity;
            if (above <= 0.0F) {
                continue;
            }
            unsigned char* pixel =
                &pixels[((static_cast<std::size_t>(y) * width) + static_cast<std::size_t>(x)) * 4];
            const float below = static_cast<float>(pixel[3]) / 255.0F;
            const float alpha = above + (below * (1.0F - above));
            for (std::size_t channel = 0; channel < 3; ++channel) {
                const float mixed =
                    ((static_cast<float>(source[channel]) * above) +
                     (static_cast<float>(pixel[channel]) * below * (1.0F - above))) /
                    alpha;
                pixel[channel] = static_cast<unsigned char>(std::lround(mixed));
            }
            pixel[3] = static_cast<unsigned char>(std::lround(alpha * 255.0F));
        }
    }
}

ArtSource::ArtSource(std::filesystem::path directory) : directory_{std::move(directory)} {
}

Art ArtSource::svg(const char* file, const Box& box, std::span<const svg::ColourSwap> swaps) const {
    const int width = std::max(1, static_cast<int>(std::lround(box.width)));
    const int height = std::max(1, static_cast<int>(std::lround(box.height)));
    std::optional<std::vector<unsigned char>> rgba =
        svg::rasterise(directory_ / file, width, height, swaps);
    if (!rgba) {
        return {};
    }
    return Art{std::move(*rgba), width, height};
}

} // namespace opensu::touch
