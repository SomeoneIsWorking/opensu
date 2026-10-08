#include "icon_recolour.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace iideck::ui {
namespace {

// The source icon's own gradient, read off `res/TQ.png`'s centre column: brightest (v 1) at the top
// in cyan, v 0.69 in blue where it ends, and v 0.47 in the dark base under the body.
constexpr float gradientEndBrightness = 0.69f;
constexpr float baseFloorBrightness = 0.30f;
// The cyan at the top is not fully saturated (62, 249, 255).
constexpr float topSaturation = 0.76f;

float lerp(float from, float to, float t) noexcept {
    return from + (to - from) * t;
}

} // namespace

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

void recolourIcon(std::span<std::uint8_t> rgba, int width, int height, Rgb from, Rgb to) noexcept {
    if (width <= 0 || height <= 0) {
        return;
    }
    const float source[3] = {static_cast<float>(from.r), static_cast<float>(from.g),
                             static_cast<float>(from.b)};
    const float target[3] = {static_cast<float>(to.r), static_cast<float>(to.g),
                             static_cast<float>(to.b)};
    for (std::size_t i = 0; i + 3 < rgba.size(); i += 4) {
        const float highest = static_cast<float>(std::max({rgba[i], rgba[i + 1], rgba[i + 2]}));
        const float lowest = static_cast<float>(std::min({rgba[i], rgba[i + 1], rgba[i + 2]}));
        const float brightness = highest / 255.0f;
        const float saturation =
            highest > 0.0f ? std::clamp((highest - lowest) / highest / topSaturation, 0.0f, 1.0f)
                           : 0.0f;
        // Where the pixel sits in the source's gradient, and how far into the dark base.
        const float along =
            std::clamp((1.0f - brightness) / (1.0f - gradientEndBrightness), 0.0f, 1.0f);
        const float shade = brightness >= gradientEndBrightness
                                ? 1.0f
                                : std::clamp((brightness - baseFloorBrightness) /
                                                 (gradientEndBrightness - baseFloorBrightness),
                                             0.0f, 1.0f);
        for (std::size_t channel = 0; channel < 3; ++channel) {
            const float gradient = lerp(source[channel], target[channel], along) * shade;
            rgba[i + channel] = static_cast<std::uint8_t>(
                std::clamp(lerp(255.0f, gradient, saturation) + 0.5f, 0.0f, 255.0f));
        }
    }
}

} // namespace iideck::ui
