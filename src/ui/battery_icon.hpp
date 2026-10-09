// battery_icon — which of iiSU's battery drawables a level shows (iiSU a32.p).
#pragma once

#include <array>
#include <cstdint>

namespace opensu::ui {

class BatteryIcon {
  public:
    /// iiSU a32.p: the drawable slot 0 (full) to 4 (emptiest) for a level.
    [[nodiscard]] static int slot(int percent, bool charging) noexcept;

    /// Bars each discharging slot shows, from res/drawable/battery_{100,80,50,20,10}.png.
    static constexpr std::array<int, 5> bars{4, 3, 2, 1, 0};
    /// Bolt colour of each charging slot, from res/drawable/battery_charg*_dark.png.
    static constexpr std::array<std::uint32_t, 5> boltColours{0x00FF48, 0x00FF48, 0xFFC000,
                                                              0xFF7200, 0xFF0000};
};

} // namespace opensu::ui
