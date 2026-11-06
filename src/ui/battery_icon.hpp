// battery_icon — what a battery level shows: the fill's width and the charging bolt's colour.
#pragma once

#include <array>
#include <cstdint>

namespace opensu::ui {

class BatteryIcon {
  public:
    /// The width of the fill in `inner` at `percent`; any level above 0 is at least `sliver`.
    [[nodiscard]] static float fillWidth(int percent, float inner, float sliver) noexcept;

    /// The bolt's colour while charging at `percent`, at iiSU a32.p's thresholds.
    [[nodiscard]] static std::uint32_t boltColour(int percent) noexcept;

  private:
    /// From res/drawable/battery_charg*_dark.png, fullest first.
    static constexpr std::array<std::uint32_t, 4> boltColours{0x00FF48, 0xFFC000, 0xFF7200,
                                                              0xFF0000};
};

} // namespace opensu::ui
