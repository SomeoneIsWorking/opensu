// save_ram_fit — a `.srm` one core of a console wrote, fitted to the save RAM another core of it
// gives, since the runner keeps one file per game for all of them (`save_paths`): the Game Boy's
// MBC2 RAM, 512 four-bit cells that mGBA packs two to a byte (0x100 bytes, the low nibble the even
// cell; `GBMBCInit`) and gambatte keeps one to a byte in an 8 KiB bank (`Mbc2`, `rambanks`), and a
// file longer than the RAM whose bytes past it hold nothing (all 0x00 or all 0xFF), such as
// gambatte's 128 KiB for a cartridge mGBA gives 64 KiB, or mGBA's 128 KiB before it knew the
// save's type.
#pragma once

#include <cstddef>
#include <optional>
#include <span>
#include <vector>

namespace opensu::retro {

/// `file` laid out for a core's save RAM of `ramBytes` bytes when the two differ in a way above;
/// nothing when it is to be loaded as it is.
[[nodiscard]] std::optional<std::vector<std::byte>> fittedSaveRam(std::span<const std::byte> file,
                                                                  std::size_t ramBytes);

} // namespace opensu::retro
