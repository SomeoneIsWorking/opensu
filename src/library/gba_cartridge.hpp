// gba_cartridge — what a GBA ROM says of its cartridge's hardware.
#pragma once

#include <string_view>

namespace opensu::library {

/// Whether the cartridge of GBA ROM `rom` has a real-time clock, which mGBA reads from the host's
/// time: its game code among those the core's overrides give one, or Nintendo's RTC library in it.
[[nodiscard]] bool hasRealTimeClock(std::string_view rom);

} // namespace opensu::library
