// icon_size — iiSU's icon size level, 1 to 20 (`xmbIconSizeLevel`, default 9), as the scale every
// Library layout sizes its tiles by (navigation.md §2.2, `w70.I`).
#pragma once

namespace opensu::ui {

inline constexpr int minIconLevel = 1;
inline constexpr int maxIconLevel = 20;
inline constexpr int defaultIconLevel = 9;

/// The level kept to its range.
[[nodiscard]] int clampIconLevel(int level) noexcept;

/// iiSU `w70.I`: `clamp((level - 10) 0.11 + 1.45, 0.67, 2.55)`, 1.34 at the default level.
[[nodiscard]] float iconScale(int level) noexcept;

/// How much larger than at the default level a tile is at `level`: `iconScale(level)` over
/// `iconScale(defaultIconLevel)`. The grid sizes its cells by it, the Carousel by `iconScale`
/// against level 10 (iiSU `lz3`).
[[nodiscard]] float relativeIconScale(int level) noexcept;

} // namespace opensu::ui
