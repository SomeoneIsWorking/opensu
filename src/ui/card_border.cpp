#include "card_border.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace opensu::ui {

Gradient borderColours(std::span<const std::uint8_t> rgba, int width, int height) noexcept {
    if (width <= 0 || height <= 0 ||
        rgba.size() < static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4) {
        return Gradient{};
    }
    const auto columns = static_cast<std::size_t>(width);
    const auto average = [&](int row) {
        constexpr int half = 4;
        const int centre = width / 2;
        int sums[3] = {0, 0, 0};
        int count = 0;
        for (int x = std::max(centre - half, 0); x <= std::min(centre + half, width - 1); ++x) {
            const std::size_t at =
                (static_cast<std::size_t>(row) * columns + static_cast<std::size_t>(x)) * 4;
            for (std::size_t channel = 0; channel < 3; ++channel) {
                sums[channel] += rgba[at + channel];
            }
            ++count;
        }
        return Rgb{static_cast<std::uint8_t>(sums[0] / count),
                   static_cast<std::uint8_t>(sums[1] / count),
                   static_cast<std::uint8_t>(sums[2] / count)};
    };
    const int inset = std::clamp(
        static_cast<int>(std::lround(13.0f / 1024.0f * static_cast<float>(height))), 0, height - 1);
    return Gradient{average(inset), average(height - 1 - inset)};
}

} // namespace opensu::ui
