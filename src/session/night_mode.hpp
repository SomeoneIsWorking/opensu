// night_mode — a colour temperature as the Gamescope fork's night mode: the blackbody white point
// at that temperature (Planckian locus in linear sRGB, balanced so 6500 K is white, scaled to keep
// luminance, gamma encoded), given as GAMESCOPE_COLOR_NIGHT_MODE's amount, hue, saturation and
// value. Gamescope turns those back into that white point and multiplies it, linearised, into every
// pixel, clipping a channel past full, as KWin's night light does.
#pragma once

#include <array>
#include <cstdint>

namespace opensu::session {

/// The neutral temperature: no warming at or above it.
inline constexpr int neutralKelvin = 6500;
/// The warmest temperature the table holds.
inline constexpr int warmestKelvin = 1000;

/// The gamma-encoded white point of `kelvin`, red first, with the luminance of white (so red goes
/// past 1); white at `neutralKelvin` and above, clamped at `warmestKelvin`.
[[nodiscard]] std::array<float, 3> whitePointOf(int kelvin) noexcept;

/// Gamescope's night mode: the white point as HSV, scaled by `amount`.
struct NightMode {
    float amount{0.0F};
    float hue{0.0F};
    float saturation{0.0F};
    float value{1.0F};

    /// The four CARDINALs of GAMESCOPE_COLOR_NIGHT_MODE: each float's bits.
    [[nodiscard]] std::array<std::uint32_t, 4> atom() const noexcept;
};

/// The night mode that shows `kelvin`; off (amount 0) at `neutralKelvin` and above.
[[nodiscard]] NightMode nightModeOf(int kelvin) noexcept;

} // namespace opensu::session
