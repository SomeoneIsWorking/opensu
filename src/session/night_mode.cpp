#include "night_mode.hpp"

#include <algorithm>
#include <bit>
#include <cmath>

namespace opensu::session {
namespace {

using Rgb = std::array<double, 3>;

/// Linear sRGB of the blackbody at `kelvin`, luminance 1: Krystek's 1985 rational fit of the
/// Planckian locus in CIE 1960 uv (1000 K to 15000 K), then CIE xy, XYZ and IEC 61966-2-1's matrix.
Rgb blackbodyLinear(double kelvin) noexcept {
    const double t = kelvin;
    const double u = (0.860117757 + 1.54118254e-4 * t + 1.28641212e-7 * t * t) /
                     (1.0 + 8.42420235e-4 * t + 7.08145163e-7 * t * t);
    const double v = (0.317398726 + 4.22806245e-5 * t + 4.20481691e-8 * t * t) /
                     (1.0 - 2.89741816e-5 * t + 1.61456053e-7 * t * t);
    const double d = 2.0 * u - 8.0 * v + 4.0;
    const double x = 3.0 * u / d;
    const double y = 2.0 * v / d;
    const double bigX = x / y;
    const double bigZ = (1.0 - x - y) / y;
    return {3.2406 * bigX - 1.5372 - 0.4986 * bigZ, -0.9689 * bigX + 1.8758 + 0.0415 * bigZ,
            0.0557 * bigX - 0.2040 + 1.0570 * bigZ};
}

} // namespace

std::array<float, 3> whitePointOf(int kelvin) noexcept {
    if (kelvin >= neutralKelvin) {
        return {1.0F, 1.0F, 1.0F};
    }
    // Balanced so the neutral temperature is white, as KWin's night light is.
    const Rgb warm = blackbodyLinear(std::max(kelvin, warmestKelvin));
    const Rgb neutral = blackbodyLinear(neutralKelvin);
    Rgb balanced{};
    for (std::size_t channel = 0; channel < balanced.size(); ++channel) {
        balanced.at(channel) = std::max(warm.at(channel) / neutral.at(channel), 0.0);
    }
    // Rec. 709 luminance: the warm white is as bright as white, as KWin's night light keeps it.
    const double luminance = 0.2126 * balanced[0] + 0.7152 * balanced[1] + 0.0722 * balanced[2];
    // Gamma 2.2 encoded, the form Gamescope's night mode takes and linearises again.
    constexpr double encode = 1.0 / 2.2;
    return {static_cast<float>(std::pow(balanced[0] / luminance, encode)),
            static_cast<float>(std::pow(balanced[1] / luminance, encode)),
            static_cast<float>(std::pow(balanced[2] / luminance, encode))};
}

std::array<std::uint32_t, 4> NightMode::atom() const noexcept {
    return {std::bit_cast<std::uint32_t>(amount), std::bit_cast<std::uint32_t>(hue),
            std::bit_cast<std::uint32_t>(saturation), std::bit_cast<std::uint32_t>(value)};
}

NightMode nightModeOf(int kelvin) noexcept {
    if (kelvin >= neutralKelvin) {
        return {};
    }
    // Gamescope's rgb_to_hsv.
    const auto [red, green, blue] = whitePointOf(kelvin);
    const float high = std::max({red, green, blue});
    const float low = std::min({red, green, blue});
    const float delta = high - low;
    if (delta <= 0.0F) {
        return {};
    }
    float hue = 0.0F;
    if (red == high) {
        hue = (green - blue) / delta;
    } else if (green == high) {
        hue = 2.0F + (blue - red) / delta;
    } else {
        hue = 4.0F + (red - green) / delta;
    }
    hue /= 6.0F;
    if (hue < 0.0F) {
        hue += 1.0F;
    }
    return {.amount = 1.0F, .hue = hue, .saturation = delta / high, .value = high};
}

} // namespace opensu::session
